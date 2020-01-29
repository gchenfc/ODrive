import numpy as np
import matplotlib.pyplot as plt
import sys

if len(sys.argv) > 1:
    fname = sys.argv[1]
else:
    fname = 'cogging_measure_final_a_4'
data_raw = np.load(fname+'.npy')

data = data_raw[0::16]
# data = np.roll(data, 256)

encoder_cpr = 8192/16
stator_slots = 12
pole_pairs = 7

N = data.size
fft = np.fft.rfft(data)
freq = np.fft.rfftfreq(N, d=1./encoder_cpr)

harmonics = [0]
harmonics += [(i+1)*stator_slots for i in range(pole_pairs)]
harmonics += [pole_pairs]
harmonics += [(i+1)*2*pole_pairs for i in range(int(stator_slots/4))]

fft_sparse = fft.copy()
indicies = np.arange(fft_sparse.size)
mask = [i not in harmonics for i in indicies]
fft_sparse[mask] = 0.0
interp_data = np.fft.irfft(fft_sparse, n = 8192) * 16

#%%

plt.figure(figsize=(14, 7))
# plt.subplot(3, 1, 1)
plt.plot(np.arange(0, 8192, 16), data, label='raw')
plt.plot(interp_data, label='selected harmonics IFFT')
plt.title('cogging map')
plt.xlabel('counts')
plt.ylabel('A')
plt.legend(loc='best')
# plt.ylim([-3, 3])

plt.savefig(fname)
np.save(fname+'_interp.npy', interp_data)
#plt.figure()
# plt.subplot(3, 1, 2)
# plt.stem(freq, np.abs(fft)/N, label='raw')
# plt.stem(freq[harmonics], np.abs(fft_sparse[harmonics])/N, markerfmt='ro', label='selected harmonics')
# plt.title('cogging map spectrum')
# plt.xlabel('cycles/turn')
# plt.ylabel('A')
# plt.legend(loc='best')

# #plt.figure()
# plt.subplot(3, 1, 3)
# plt.stem(freq, np.abs(fft)/N, label='raw')
# plt.stem(freq[harmonics], np.abs(fft_sparse[harmonics])/N, markerfmt='ro', label='selected harmonics')
# plt.title('cogging map spectrum')
# plt.xlabel('cycles/turn')
# plt.ylabel('A')
# plt.legend(loc='best')

plt.show()