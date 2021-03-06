
#include <stdlib.h>
#include <functional>
#include "gpio.h"

#include "communication/interface_can.hpp"
#include "odrive_main.h"
#include "utils.h"

Axis::Axis(int axis_num,
           const AxisHardwareConfig_t& hw_config,
           Config_t& config,
           Encoder& encoder,
           SensorlessEstimator& sensorless_estimator,
           Controller& controller,
           Motor& motor,
           TrapezoidalTrajectory& trap,
           Endstop& min_endstop,
           Endstop& max_endstop)
    : axis_num_(axis_num),
      hw_config_(hw_config),
      config_(config),
      encoder_(encoder),
      sensorless_estimator_(sensorless_estimator),
      controller_(controller),
      motor_(motor),
      trap_(trap),
      min_endstop_(min_endstop),
      max_endstop_(max_endstop) {
    encoder_.axis_              = this;
    sensorless_estimator_.axis_ = this;
    controller_.axis_           = this;
    motor_.axis_                = this;
    trap_.axis_                 = this;
    decode_step_dir_pins();
    watchdog_feed();
    min_endstop_.axis_ = this;
    max_endstop_.axis_ = this;
}

Axis::LockinConfig_t Axis::default_calibration() {
    Axis::LockinConfig_t config;
    config.current            = 10.0f;                 // [A]
    config.ramp_time          = 0.4f;                  // [s]
    config.ramp_distance      = 1 * M_PI;              // [rad]
    config.accel              = 20.0f;                 // [rad/s^2]
    config.vel                = 40.0f;                 // [rad/s]
    config.finish_distance    = 100.0f * 2.0f * M_PI;  // [rad]
    config.finish_on_vel      = false;
    config.finish_on_distance = true;
    config.finish_on_enc_idx  = true;
    return config;
}

Axis::LockinConfig_t Axis::default_sensorless() {
    Axis::LockinConfig_t config;
    config.current            = 10.0f;     // [A]
    config.ramp_time          = 0.4f;      // [s]
    config.ramp_distance      = 1 * M_PI;  // [rad]
    config.accel              = 200.0f;    // [rad/s^2]
    config.vel                = 400.0f;    // [rad/s]
    config.finish_distance    = 100.0f;    // [rad]
    config.finish_on_vel      = true;
    config.finish_on_distance = false;
    config.finish_on_enc_idx  = false;
    return config;
}

static void step_cb_wrapper(void* ctx) {
    reinterpret_cast<Axis*>(ctx)->step_cb();
}

// @brief Sets up all components of the axis,
// such as gate driver and encoder hardware.
void Axis::setup() {
    encoder_.setup();
    motor_.setup();
}

static void run_state_machine_loop_wrapper(void* ctx) {
    reinterpret_cast<Axis*>(ctx)->run_state_machine_loop();
    reinterpret_cast<Axis*>(ctx)->thread_id_valid_ = false;
}

// @brief Starts run_state_machine_loop in a new thread
void Axis::start_thread() {
    osThreadDef(thread_def, run_state_machine_loop_wrapper, hw_config_.thread_priority, 0, stack_size_ / sizeof(StackType_t));
    thread_id_       = osThreadCreate(osThread(thread_def), this);
    thread_id_valid_ = true;
}

// @brief Unblocks the control loop thread.
// This is called from the current sense interrupt handler.
void Axis::signal_current_meas() {
    if (thread_id_valid_)
        osSignalSet(thread_id_, M_SIGNAL_PH_CURRENT_MEAS);
}

// @brief Blocks until a current measurement is completed
// @returns True on success, false otherwise
bool Axis::wait_for_current_meas() {
    return osSignalWait(M_SIGNAL_PH_CURRENT_MEAS, PH_CURRENT_MEAS_TIMEOUT).status == osEventSignal;
}

// step/direction interface
void Axis::step_cb() {
    if (step_dir_active_) {
        GPIO_PinState dir_pin = HAL_GPIO_ReadPin(dir_port_, dir_pin_);
        float dir             = (dir_pin == GPIO_PIN_SET) ? 1.0f : -1.0f;
        controller_.input_pos_ += dir * config_.counts_per_step;
        controller_.input_pos_updated();
    }
};

void Axis::load_default_step_dir_pin_config(
    const AxisHardwareConfig_t& hw_config, Config_t* config) {
    config->step_gpio_pin = hw_config.step_gpio_pin;
    config->dir_gpio_pin  = hw_config.dir_gpio_pin;
}

void Axis::load_default_can_id(const int& id, Config_t& config) {
    config.can_node_id = id;
}

void Axis::decode_step_dir_pins() {
    step_port_ = get_gpio_port_by_pin(config_.step_gpio_pin);
    step_pin_  = get_gpio_pin_by_pin(config_.step_gpio_pin);
    dir_port_  = get_gpio_port_by_pin(config_.dir_gpio_pin);
    dir_pin_   = get_gpio_pin_by_pin(config_.dir_gpio_pin);
}

// @brief (de)activates step/dir input
void Axis::set_step_dir_active(bool active) {
    if (active) {
        // Set up the direction GPIO as input
        GPIO_InitTypeDef GPIO_InitStruct;
        GPIO_InitStruct.Pin  = dir_pin_;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(dir_port_, &GPIO_InitStruct);

        // Subscribe to rising edges of the step GPIO
        GPIO_subscribe(step_port_, step_pin_, GPIO_PULLDOWN, step_cb_wrapper, this);

        step_dir_active_ = true;
    } else {
        step_dir_active_ = false;

        // Unsubscribe from step GPIO
        GPIO_unsubscribe(step_port_, step_pin_);
    }
}

// @brief Do axis level checks and call subcomponent do_checks
// Returns true if everything is ok.
bool Axis::do_checks() {
    if (!brake_resistor_armed)
        error_ |= ERROR_BRAKE_RESISTOR_DISARMED;
    if ((current_state_ != AXIS_STATE_IDLE) && (motor_.armed_state_ == Motor::ARMED_STATE_DISARMED))
        // motor got disarmed in something other than the idle loop
        error_ |= ERROR_MOTOR_DISARMED;
    if (!(vbus_voltage >= board_config.dc_bus_undervoltage_trip_level))
        error_ |= ERROR_DC_BUS_UNDER_VOLTAGE;
    if (!(vbus_voltage <= board_config.dc_bus_overvoltage_trip_level))
        error_ |= ERROR_DC_BUS_OVER_VOLTAGE;

    // This is the same math that's used in update_brake_current().  Should we calculate IBus globally?
    float Ibus_sum = 0.0f;
    for (size_t i = 0; i < AXIS_COUNT; ++i) {
        if (axes[i]->motor_.armed_state_ == Motor::ARMED_STATE_ARMED) {
            Ibus_sum += axes[i]->motor_.current_control_.Ibus;
        }
    }

    if (board_config.power_supply_wattage > 0.0f &&
        (Ibus_sum * vbus_voltage) > board_config.power_supply_wattage) {
        error_ |= ERROR_DC_BUS_OVER_POWER;
    }

    // Sub-components should use set_error which will propegate to this error_
    motor_.do_checks();
    encoder_.do_checks();
    // sensorless_estimator_.do_checks();
    // controller_.do_checks();

    return check_for_errors();
}

// @brief Update all esitmators
bool Axis::do_updates() {
    // Sub-components should use set_error which will propegate to this error_
    encoder_.update();
    sensorless_estimator_.update();
    min_endstop_.update();
    max_endstop_.update();
    bool ret = check_for_errors();
    odCAN->send_heartbeat(this);
    return ret;
}

// @brief Feed the watchdog to prevent watchdog timeouts.
void Axis::watchdog_feed() {
    watchdog_current_value_ = get_watchdog_reset();
}

// @brief Check the watchdog timer for expiration. Also sets the watchdog error bit if expired.
bool Axis::watchdog_check() {
    // reset value = 0 means watchdog disabled.
    if (!config_.enable_watchdog) return true;
    if (get_watchdog_reset() == 0) return true;

    // explicit check here to ensure that we don't underflow back to UINT32_MAX
    if (watchdog_current_value_ > 0) {
        watchdog_current_value_--;
        return true;
    } else {
        error_ |= ERROR_WATCHDOG_TIMER_EXPIRED;
        return false;
    }
}

bool Axis::run_lockin_spin(const LockinConfig_t& lockin_config) {
    // Spiral up current for softer rotor lock-in
    lockin_state_ = LOCKIN_STATE_RAMP;
    float x       = 0.0f;
    run_control_loop([&]() {
        float phase = wrap_pm_pi(lockin_config.ramp_distance * x);
        float I_mag = lockin_config.current * x;
        x += current_meas_period / lockin_config.ramp_time;
        if (!motor_.update(I_mag, phase, 0.0f))
            return false;
        return x < 1.0f;
    });

    // Spin states
    float distance = lockin_config.ramp_distance;
    float phase    = wrap_pm_pi(distance);
    float vel      = distance / lockin_config.ramp_time;

    // Function of states to check if we are done
    auto spin_done = [&](bool vel_override = false) -> bool {
        bool done = false;
        if (lockin_config.finish_on_vel || vel_override)
            done = done || std::abs(vel) >= std::abs(lockin_config.vel);
        if (lockin_config.finish_on_distance)
            done = done || std::abs(distance) >= std::abs(lockin_config.finish_distance);
        if (lockin_config.finish_on_enc_idx)
            done = done || encoder_.index_found_;
        return done;
    };

    // Accelerate
    lockin_state_ = LOCKIN_STATE_ACCELERATE;
    run_control_loop([&]() {
        vel += lockin_config.accel * current_meas_period;
        distance += vel * current_meas_period;
        phase = wrap_pm_pi(phase + vel * current_meas_period);

        if (!motor_.update(lockin_config.current, phase, vel))
            return false;
        return !spin_done(true);  //vel_override to go to next phase
    });

    if (!encoder_.index_found_)
        encoder_.set_idx_subscribe(true);

    // Constant speed
    if (!spin_done()) {
        lockin_state_ = LOCKIN_STATE_CONST_VEL;
        vel           = lockin_config.vel;  // reset to actual specified vel to avoid small integration error
        run_control_loop([&]() {
            distance += vel * current_meas_period;
            phase = wrap_pm_pi(phase + vel * current_meas_period);

            if (!motor_.update(lockin_config.current, phase, vel))
                return false;
            return !spin_done();
        });
    }

    lockin_state_ = LOCKIN_STATE_INACTIVE;
    return check_for_errors();
}

// Note run_sensorless_control_loop and run_closed_loop_control_loop are very similar and differ only in where we get the estimate from.
bool Axis::run_sensorless_control_loop() {
    run_control_loop([this]() {
        if (controller_.config_.control_mode >= Controller::CTRL_MODE_POSITION_CONTROL)
            return error_ |= ERROR_POS_CTRL_DURING_SENSORLESS, false;

        // Note that all estimators are updated in the loop prefix in run_control_loop
        float current_setpoint;
        if (!controller_.update(sensorless_estimator_.pll_pos_, sensorless_estimator_.vel_estimate_, &current_setpoint))
            return error_ |= ERROR_CONTROLLER_FAILED, false;
        if (!motor_.update(current_setpoint, sensorless_estimator_.phase_, sensorless_estimator_.vel_estimate_))
            return false;  // set_error should update axis.error_
        return true;
    });
    return check_for_errors();
}

bool Axis::run_closed_loop_control_loop() {
    // To avoid any transient on startup, we intialize the setpoint to be the current position
    controller_.pos_setpoint_ = encoder_.pos_estimate_;

    // Avoid integrator windup issues
    controller_.vel_integrator_current_ = 0.0f;

    set_step_dir_active(config_.enable_step_dir);
    run_control_loop([this]() {
        // Note that all estimators are updated in the loop prefix in run_control_loop
        float current_setpoint;
        if (controller_.config_.use_load_encoder) {
            if (controller_.config_.load_encoder_axis < AXIS_COUNT) {
                Axis* ax = axes[controller_.config_.load_encoder_axis];
                if (!controller_.update(ax->encoder_.pos_estimate_, encoder_.vel_estimate_, &current_setpoint))
                    return error_ |= ERROR_CONTROLLER_FAILED, false;
            } else{
                controller_.set_error(Controller::ERROR_INVALID_LOAD_ENCODER);
                return error_ |= ERROR_CONTROLLER_FAILED, false;
            }
        } else if (!controller_.update(encoder_.pos_estimate_, encoder_.vel_estimate_, &current_setpoint))
            return error_ |= ERROR_CONTROLLER_FAILED, false;  //TODO: Make controller.set_error
        float phase_vel = 2 * M_PI * encoder_.vel_estimate_ / (float)encoder_.config_.cpr * motor_.config_.pole_pairs;
        if (!motor_.update(current_setpoint, encoder_.phase_, phase_vel))
            return false;  // set_error should update axis.error_

        // Handle the homing case
        if (homing_.homing_state == HOMING_STATE_HOMING) {
            if (min_endstop_.getEndstopState()) {
                // pos_setpoint is the starting position for the trap_traj so we need to set it.
                controller_.pos_setpoint_ = min_endstop_.config_.offset;
                controller_.vel_setpoint_ = 0.0f;  // Change directions without decelerating

                // Set our current position in encoder counts to make control more logical
                encoder_.set_linear_count(static_cast<int32_t>(controller_.pos_setpoint_));

                controller_.config_.control_mode = Controller::CTRL_MODE_POSITION_CONTROL;
                controller_.config_.input_mode   = Controller::INPUT_MODE_TRAP_TRAJ;

                controller_.input_pos_ = 0.0f;
                controller_.input_pos_updated();
                controller_.input_vel_     = 0.0f;
                controller_.input_current_ = 0.0f;

                homing_.homing_state = HOMING_STATE_MOVE_TO_ZERO;
            }
        } else if (homing_.homing_state == HOMING_STATE_MOVE_TO_ZERO) {
            if (!min_endstop_.getEndstopState() && controller_.trajectory_done_) {
                controller_.config_.control_mode = homing_.storedControlMode;
                controller_.config_.input_mode   = homing_.storedInputMode;
                homing_.homing_state             = HOMING_STATE_IDLE;
                homing_.isHomed                  = true;
            }
        } else {
            // Check for endstop presses
            if (min_endstop_.config_.enabled && min_endstop_.getEndstopState()) {
                return error_ |= ERROR_MIN_ENDSTOP_PRESSED, false;
            } else if (max_endstop_.config_.enabled && max_endstop_.getEndstopState()) {
                return error_ |= ERROR_MAX_ENDSTOP_PRESSED, false;
            }
        }
        return true;
    });
    set_step_dir_active(false);
    return check_for_errors();
}

bool Axis::run_idle_loop() {
    // run_control_loop ignores missed modulation timing updates
    // if and only if we're in AXIS_STATE_IDLE
    safety_critical_disarm_motor_pwm(motor_);
    run_control_loop([this]() {
        return true;
    });
    return check_for_errors();
}

// Infinite loop that does calibration and enters main control loop as appropriate
void Axis::run_state_machine_loop() {
    // arm!
    motor_.arm();

    for (;;) {
        // Load the task chain if a specific request is pending
        if (requested_state_ != AXIS_STATE_UNDEFINED) {
            size_t pos = 0;
            if (requested_state_ == AXIS_STATE_STARTUP_SEQUENCE) {
                if (config_.startup_motor_calibration)
                    task_chain_[pos++] = AXIS_STATE_MOTOR_CALIBRATION;
                if (config_.startup_encoder_index_search && encoder_.config_.use_index)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_INDEX_SEARCH;
                if (config_.startup_encoder_offset_calibration)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_OFFSET_CALIBRATION;
                if (config_.startup_closed_loop_control) {
                    if (config_.startup_homing)
                        task_chain_[pos++] = AXIS_STATE_HOMING;
                    task_chain_[pos++] = AXIS_STATE_CLOSED_LOOP_CONTROL;
                } else if (config_.startup_sensorless_control)
                    task_chain_[pos++] = AXIS_STATE_SENSORLESS_CONTROL;
                task_chain_[pos++] = AXIS_STATE_IDLE;
            } else if (requested_state_ == AXIS_STATE_FULL_CALIBRATION_SEQUENCE) {
                task_chain_[pos++] = AXIS_STATE_MOTOR_CALIBRATION;
                if (encoder_.config_.use_index)
                    task_chain_[pos++] = AXIS_STATE_ENCODER_INDEX_SEARCH;
                task_chain_[pos++] = AXIS_STATE_ENCODER_OFFSET_CALIBRATION;
                task_chain_[pos++] = AXIS_STATE_IDLE;
            } else if (requested_state_ != AXIS_STATE_UNDEFINED) {
                task_chain_[pos++] = requested_state_;
                task_chain_[pos++] = AXIS_STATE_IDLE;
            }
            task_chain_[pos++] = AXIS_STATE_UNDEFINED;  // TODO: bounds checking
            requested_state_   = AXIS_STATE_UNDEFINED;
            // Auto-clear any invalid state error
            error_ &= ~ERROR_INVALID_STATE;
        }

        // Note that current_state is a reference to task_chain_[0]

        // Run the specified state
        // Handlers should exit if requested_state != AXIS_STATE_UNDEFINED
        bool status;
        switch (current_state_) {
            case AXIS_STATE_MOTOR_CALIBRATION: {
                status = motor_.run_calibration();
            } break;

            case AXIS_STATE_ENCODER_INDEX_SEARCH: {
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;
                if (encoder_.config_.idx_search_unidirectional && motor_.config_.direction == 0)
                    goto invalid_state_label;

                status = encoder_.run_index_search();
            } break;

            case AXIS_STATE_ENCODER_DIR_FIND: {
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;

                status = encoder_.run_direction_find();
            } break;

            case AXIS_STATE_HOMING:
                status = controller_.home_axis();
                break;

            case AXIS_STATE_ENCODER_OFFSET_CALIBRATION: {
                if (!motor_.is_calibrated_)
                    goto invalid_state_label;
                status = encoder_.run_offset_calibration();
            } break;

            case AXIS_STATE_LOCKIN_SPIN: {
                if (!motor_.is_calibrated_ || motor_.config_.direction == 0)
                    goto invalid_state_label;
                status = run_lockin_spin(config_.lockin);
            } break;

            case AXIS_STATE_SENSORLESS_CONTROL: {
                if (!motor_.is_calibrated_ || motor_.config_.direction == 0)
                    goto invalid_state_label;
                status = run_lockin_spin(config_.sensorless_ramp);  // TODO: restart if desired
                if (status) {
                    // call to controller.reset() that happend when arming means that vel_setpoint
                    // is zeroed. So we make the setpoint the spinup target for smooth transition.
                    controller_.vel_setpoint_ = config_.sensorless_ramp.vel;
                    status                    = run_sensorless_control_loop();
                }
            } break;

            case AXIS_STATE_CLOSED_LOOP_CONTROL: {
                if (!motor_.is_calibrated_ || motor_.config_.direction == 0)
                    goto invalid_state_label;
                if (!encoder_.is_ready_)
                    goto invalid_state_label;
                status = run_closed_loop_control_loop();
            } break;

            case AXIS_STATE_IDLE: {
                run_idle_loop();
                status = motor_.arm();  // done with idling - try to arm the motor
            } break;

            default:
            invalid_state_label:
                error_ |= ERROR_INVALID_STATE;
                status = false;  // this will set the state to idle
                break;
        }

        // If the state failed, go to idle, else advance task chain
        if (!status)
            current_state_ = AXIS_STATE_IDLE;
        else
            memmove(task_chain_, task_chain_ + 1, sizeof(task_chain_) - sizeof(task_chain_[0]));
    }
}
