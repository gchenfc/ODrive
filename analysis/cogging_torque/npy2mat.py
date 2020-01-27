"""Converts numpy files to matlab files in a directory.
Provide the folder name as a command line argument.
"""
import glob
import numpy as np
import scipy.io as sio
import sys

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage:\n\tpython npy2mat.py [folder name]')
        exit(0)
    folder = sys.argv[1]
    filenames = glob.glob(folder+'/*.npy')
    print('found {:d} files...'.format(len(filenames)))
    for i, filename in enumerate(filenames):
        print('converting file {:d} of {:d}'.format(i, len(filenames)))
        data = np.load(filename)
        sio.savemat(filename[:-4] + '.mat', {'data':data})
    print('done')