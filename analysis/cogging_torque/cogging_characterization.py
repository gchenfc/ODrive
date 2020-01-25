import numpy as np
import time

"""useful commands
start_liveplotter(lambda:[odrv0.axis0.motor.current_control.Iq_measured, ((odrv0.axis0.controller.pos_setpoint-odrv0.axis0.encoder.count_in_cpr + 4096) % 8192 - 4096)/10])

odrv0.axis0.motor.current_control.I_measured_report_filter_k = 0.001
odrv0.axis0.motor.config.current_control_bandwidth = 500
odrv0.axis0.motor.config.current_lim_tolerance = 10
from importlib import reload
sys.path.append('.')

with drum:
pos_gain = 100 (float)
vel_gain = 0.0007140250527299941 (float)
vel_integrator_gain = 0.004641162697225809 (float)

without drum:
odrv0.axis0.controller.config.pos_gain = 100
odrv0.axis0.controller.config.vel_gain = 0.00019999999494757503
odrv0.axis0.controller.config.vel_integrator_gain = 0.0009999999310821295       
"""
def find_cog(odrv0, step=4*4, waittime=3, offset=0, a=None, reverse=False):
    if a is None:
        a = np.zeros(8192)
    backup_i = 0
    for i in range(offset, 8192, step):
        print("testing step {:d}".format(i))
        odrv0.axis0.controller.set_pos_setpoint(8191-i if reverse else i, 0, 0) 
        time.sleep(waittime)
        a[8191-i if reverse else i] = odrv0.axis0.motor.current_control.Iq_measured
        backup_i = backup_i + 1
        if (backup_i >= 10):
            backup_i = 0
            print('backing up...')
            np.save('bak/cogging_measure_{:03d}'.format(8191-i if reverse else i), a)
    np.save('cogging_measure_final'.format(backup_i), a)
    return a

def write_anticogging_map(odrv0, map):
    for i in range(8192):
        odrv0.axis0.controller.anticogging.write_anticogging_map(i, map[i])

def read_anticogging_map(odrv0, inds=range(8192)):
    amap = []
    nan = float('nan')
    for i in inds:
        amap.append(odrv0.axis0.controller.anticogging.write_anticogging_map(i, nan))
    return amap

def slowveltest(odrv0, vel=1, rev=1, fname='slow'):
    ts = []
    poss = []
    curs = []
    odrv0.axis0.controller.set_pos_setpoint(0, 0, 0)
    start_t = time.time()
    while time.time() < (start_t + rev/vel):
        t_, pos_, cur_ = time.time()-start_t, odrv0.axis0.encoder.pos_estimate, odrv0.axis0.motor.current_control.Iq_measured
        ts.append(t_)
        poss.append(pos_)
        curs.append(cur_)
        odrv0.axis0.controller.set_pos_setpoint(vel * (time.time()-start_t) * 8192, 0, 0)
    np.save(fname, [ts, poss, curs])
    odrv0.axis0.controller.set_pos_setpoint(0, 0, 0)
    return ts, poss, curs

def slowveltest2(odrv0, vel=1, rev=1, fname='slow2'):
    ts = []
    poss = []
    curs = []
    odrv0.axis0.controller.set_vel_setpoint(vel, 0)
    start_t = time.time()
    while time.time() < (start_t + rev/vel):
        t_, pos_, cur_ = time.time()-start_t, odrv0.axis0.encoder.pos_estimate, odrv0.axis0.motor.current_control.Iq_measured
        ts.append(t_)
        poss.append(pos_)
        curs.append(cur_)
    np.save(fname, [ts, poss, curs])
    odrv0.axis0.controller.set_pos_setpoint(0, 0, 0)
    return ts, poss, curs

def spinuptest(odrv0, target_pos = 10000, current = 1):
    t = []
    pos = []
    cur = []
    start_t = time.time()
    for i in range(25):
        t_, pos_, cur_ = time.time()-start_t, odrv0.axis0.encoder.pos_estimate, odrv0.axis0.motor.current_control.Iq_measured
        t.append(t_)
        pos.append(pos_)
        cur.append(cur_)
    odrv0.axis0.controller.set_current_setpoint(current) 
    while ((current > 0 and odrv0.axis0.encoder.pos_estimate < target_pos) or 
           (current < 0 and odrv0.axis0.encoder.pos_estimate > target_pos)):
        t_, pos_, cur_ = time.time()-start_t, odrv0.axis0.encoder.pos_estimate, odrv0.axis0.motor.current_control.Iq_measured
        t.append(t_)
        pos.append(pos_)
        cur.append(cur_)
        # print("{:5.2f}\t{:5.2f}\t{:5.2f}".format(t_, pos_, cur_)) 
        if t_ > 5:
            break
    odrv0.axis0.controller.set_current_setpoint(0)
    return t, pos, cur

def oscillatetest(odrv0, freq=100, current=1, count=10):
    start_t = time.time()
    for i in range(count):
        start_t += 0.5/freq
        odrv0.axis0.controller.set_current_setpoint(current)
        time.sleep(max(start_t - time.time(), 0.00001))
        start_t += 0.5/freq
        odrv0.axis0.controller.set_current_setpoint(-current)
        time.sleep(max(start_t - time.time(), 0.00001))
    odrv0.axis0.controller.set_current_setpoint(0)

def multiple_spinup(odrv0):
    all_setcur = []
    all_t = []
    all_pos = []
    all_cur = []
    for current in np.arange(1, 2.5, .1):
        print('doing current {:.1f}'.format(current))
        t, pos, cur = spinuptest(odrv0, 5000/current, current)
        all_setcur.append(current)
        all_t.append(t)
        all_pos.append(pos)
        all_cur.append(cur)
        t, pos, cur = spinuptest(odrv0, 0, -current)
        print('doing current {:.1f}'.format(-current))
        all_setcur.append(current)
        all_t.append(t)
        all_pos.append(pos)
        all_cur.append(cur)
    np.save('spinup', [all_setcur, all_t, all_pos, all_cur])
    return all_setcur, all_t, all_pos, all_cur