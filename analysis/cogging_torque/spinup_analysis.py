import numpy as np
import matplotlib.pyplot as plt

def main(data):

    setcur = data[0]
    t = data[1]
    pos = data[2]
    cur = data[3]

    # plot raw data
    fig = plt.figure(figsize=(10,7))
    axes = fig.subplots(2, int(len(setcur)/2), sharex='col', sharey='row')
    for i in range(len(setcur)):
        axes[0, int(i/2)].plot(t[i], pos[i])
        axes[1, int(i/2)].plot(t[i], cur[i])
        # axes[0, int(i/2)].title('Ramp for setpoint current {:3.1f}'.format("up" if i%2==0 else "down", setcur[i]))
        # plt.show()
    axes[0, 0].set_ylabel('Pos')
    axes[1, 0].set_ylabel('Cur')
    fig.suptitle('Ramp raw data')
    
    # plot acc vs cur
    fig2 = plt.figure(figsize=(6,6))
    for i in range(len(setcur)):
        def derivative(x, t):
            return np.gradient(x) / np.gradient(t)
        acc = derivative(derivative(pos[i], t[i]), t[i])
        plt.plot(cur[i], acc, 'o')

    plt.show()
    print(len(setcur))

if __name__ == '__main__':
    data = np.load('spinup.npy')
    main(data)