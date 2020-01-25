# Notes

Please see `cogging_characterization.py` for implementation details.

## Cogging map
coggingmap_0 was measured on axis 0 

*naming convention*: `cogging_measure_final_{trial}_{counts}.npy`, where `counts` is how many counts were skipped between measurements (ie go to position 0, 16, 32, ... for counts=16).  `counts` has an 'r' at the end if it was the reverse direction 

### Description of experiments
* *a* - first measurements with increasingly fine measurements.  Qualitatively, going finer than once per 16 counts
* *b* - didn't change anything, just ran again to see repeatability - very repeatable
* *c* - rebooted odrive, this time the cogging map was off by 1/2 revolution (4096 ticks)
* *d* - re"index search" on both axes with*out* rebooting odrive
* *e* - next day, measured cogging in reverse

### Results/Conclusions
![compare abcd](coggingmap_0/coggingmap_compare_abcd.gif)
![compare abcde](coggingmap_0/coggingmap_compare_abcdrev.gif)

The first gif shows the cogging maps from a-d, with c being adjusted by 180 degrees.  The second also shows e (the reversed one) and evidences some dc bias/friction.  Nevertheless, they're all very consistent.

Also, I later [(Slow tests)](#slow-tests) figured out why sometimes the cogging map is "randomly" 180 degrees off.  It's because of the other motor **crosstalk**.  See discussion in [Slow tests](#resultsconclusions-1)

## Acceleration tests
spinup the motor with set current and observe acceleration

tests 0-2 are unloaded.  Unloaded motor has moment of inertia ~0.002[units] (I forget the units, but it's roughly twice what odrive claims of their higher Kv motor).

TODO: commit analysis/plots from other computer

## Slow tests
Spin the motor slowly and see how much oscillation there is **with and without anti-cogging compensation**.

### Description
* slow_goodgains - command **position setpoints**.  first set of tests with correct (unloaded motor) gains.  setpoint 0.1 rev/s.  
    **edit actually the position gain was way too high, I forgot.  It was set at 100 instead of ~45
* slow_goodgains_2 - I don't think I changed anything
* slow2 - command **velocity** instead of position setpoints. setpoint 0.1 rev/s
* slow2_slower - move slower.  setpoint 0.03 rev/s

### Results/Conclusions
Anti-cogging totally helps and I think it's sufficient for my needs.

Videos on my phone as well corroborate.

Also, **cogging maps are 100% dependent upon adjacent motors and iron.**  It is so obvious when you turn compensation on, set current to 0, and then spin the motors with your hands.  There is one particular location of the passive motor (axis 1) that the cogging map for axis 0 was measured at and you can feel that as soon as axis 1 snaps into that position, axis 0 can spin freely.  At other locations, axis 0 cogs even more than without anti-cogging compensation.