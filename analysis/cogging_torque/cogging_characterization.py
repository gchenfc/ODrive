import numpy as np
import time

def find_cog(odrv0, step=4*4, waittime=3, offset=0, a=None):
    if a is None:
        a = np.zeros(8192)
    backup_i = 0
    for i in range(offset, 8192, step): 
        print("testing step {:d}".format(i))
        odrv0.axis0.controller.set_pos_setpoint(i, 0, 0) 
        time.sleep(waittime)
        a[i] = odrv0.axis0.motor.current_control.Iq_measured
        backup_i = backup_i + 1
        if (backup_i >= 10):
            backup_i = 0
            print('backing up...')
            np.save('bak/cogging_measure_{:03d}'.format(i), a)
    np.save('cogging_measure_final'.format(backup_i), a)
    return a

def spinuptest(odrv0, target_pos = 10000, current = 1):
    t = []
    pos = []
    cur = []
    start_t = time.time()
    odrv0.axis0.controller.set_current_setpoint(current) 
    while ((current > 0 and odrv0.axis0.encoder.pos_estimate < target_pos) or 
           (current < 0 and odrv0.axis0.encoder.pos_estimate > target_pos)):
        t_, pos_, cur_ = time.time()-start_t, odrv0.axis0.encoder.pos_estimate, odrv0.axis0.motor.current_control.Iq_measured
        t.append(t_)
        pos.append(pos_)
        cur.append(cur_)
        print("{:5.2f}\t{:5.2f}\t{:5.2f}".format(t_, pos_, cur_)) 
        if t_ > 5:
            break
    odrv0.axis0.controller.set_current_setpoint(0)
    return t, pos, cur

def multiple_spinup(odrv0):
    all_setcur = []
    all_t = []
    all_pos = []
    all_cur = []
    for current in np.arange(1.5, 3.5, .5):
        t, pos, cur = spinuptest(odrv0, 15000/current, current)
        all_setcur.append(current)
        all_t.append(t)
        all_pos.append(pos)
        all_cur.append(cur)
        t, pos, cur = spinuptest(odrv0, 0, -current)
        all_setcur.append(current)
        all_t.append(t)
        all_pos.append(pos)
        all_cur.append(cur)
    np.save('spinup', [all_setcur, all_t, all_pos, all_cur])
    return all_setcur, all_t, all_pos, all_cur