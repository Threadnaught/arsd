import matplotlib.pyplot as plt
import numpy as np
import arsd
import soundfile as sf
import os
import stat
import re

def find(path, pattern):
	if stat.S_ISDIR(os.stat(path).st_mode):
		ret = []
		for el in os.listdir(path):
			ret = ret + find(os.path.join(path, el), pattern)
		return ret
	if re.match(pattern, path):
		return [path]

	return []

# def list_fullpath(base):
# 	return np.asarray([os.path.join(base, x) for x in os.listdir(base)])

# sets = [list_fullpath('samples/many'), list_fullpath('samples/many-2')]
sets = np.asarray([find('samples/fma_medium', '.*\\.mp3')])


def pick_batch(set_i, batch_size):
	# print('picking', set_i, batch_size)
	chosen_set = sets[set_i]

	ret = chosen_set[np.random.choice(len(chosen_set), [batch_size])]
	ret[50] = 'samples/000002.mp3'
	return ret

arsd.init(pick_batch, 100, 1)

while True:
	data = arsd.draw_batch(0)
	print(data.shape)

	# sf.write('samples/clip.flac', data[10], 44100)

