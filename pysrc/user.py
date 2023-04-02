import matplotlib.pyplot as plt
import numpy as np
import arsd
import soundfile as sf
import os

def list_fullpath(base):
	return np.asarray([os.path.join(base, x) for x in os.listdir(base)])

sets = [list_fullpath('samples/many'), list_fullpath('samples/many-2')]

def pick_batch(set_i, batch_size):
	print('picking', set_i, batch_size)
	chosen_set = sets[set_i]
	return chosen_set[np.random.choice(len(chosen_set), [batch_size])]

arsd.init(pick_batch, 100, 2)

data = arsd.BLOCKING_draw_clip()

print(data.shape)
sf.write('samples/clip.flac', data[10], 44100)

