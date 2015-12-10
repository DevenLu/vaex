__author__ = 'maartenbreddels'
import os
import sys

import numpy as np

import logging
import vaex
import vaex.utils
import vaex.execution
import vaex.file.colfits

max_length = int(1e5)

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'
try:
	import h5py
except:
	if not on_rtd:
		raise
#from vaex.dataset import DatasetLocal

logger = logging.getLogger("vaex.export")

def _export(dataset_input, dataset_output, random_index_column, path, column_names=None, byteorder="=", shuffle=False, selection=False, progress=None, virtual=True):
	"""
	:param DatasetLocal dataset: dataset to export
	:param str path: path for file
	:param lis[str] column_names: list of column names to export or None for all columns
	:param str byteorder: = for native, < for little endian and > for big endian
	:param bool shuffle: export rows in random order
	:param bool selection: export selection or not
	:param progress: progress callback that gets a progress fraction as argument and should return True to continue
	:return:
	"""

	if selection:
		assert dataset_input.has_selection(), "cannot export selection is there is none"

	N = len(dataset_input) if not selection else dataset_input.selected_length()
	if N == 0:
		raise ValueError("Cannot export empty table")

	if shuffle:
		shuffle_array = dataset_output.columns[random_index_column]


	partial_shuffle = shuffle and dataset_input.full_length() != len(dataset_input)

	if partial_shuffle:
		# if we only export a portion, we need to create the full length random_index array, and
		shuffle_array_full = np.zeros(dataset_input.full_length(), dtype=byteorder+"i8")
		vaex.vaexfast.shuffled_sequence(shuffle_array_full)
		# then take a section of it
		#shuffle_array[:] = shuffle_array_full[:N]
		shuffle_array[:] = shuffle_array_full[shuffle_array_full<N]
		del shuffle_array_full
	elif shuffle:
		# better to do this in memory
		shuffle_array_memory = np.zeros_like(shuffle_array)
		vaex.vaexfast.shuffled_sequence(shuffle_array_memory)
		shuffle_array[:] = shuffle_array_memory

	#i1, i2 = 0, N #len(dataset)
	#print "creating shuffled array"
	if progress == True:
		progress = vaex.utils.progressbar_callable("exporting")
	progress = progress or (lambda value: True)
	progress_total = len(column_names) * N
	progress_value = 0
	for column_name in column_names:
		logger.debug("  exporting column: %s " % column_name)
		#with vaex.utils.Timer("copying column %s" % column_name, logger):
		if 1:
			block_scope = dataset_input._block_scope(0, vaex.execution.buffer_size)
			to_array = dataset_output.columns[column_name]
			if shuffle: # we need to create a in memory copy, otherwise we will do random writes which is VERY inefficient
				to_array_disk = to_array
				to_array = np.zeros_like(to_array_disk)
			to_offset = 0 # we need this for selections
			for i1, i2 in vaex.utils.subdivide(len(dataset_input), max_length=max_length):
				logger.debug("from %d to %d (total length: %d, output length: %d)", i1, i2, len(dataset_input), N)
				block_scope.move(i1, i2)
				#	block_scope.move(i1-i1, i2-i1)

				values = block_scope.evaluate(column_name)

				if selection:
					selection_block_length = np.sum(dataset_input.mask[i1:i2])
					to_array[to_offset:to_offset+selection_block_length] = values[dataset_input.mask[i1:i2]]
					to_offset += selection_block_length
				else:
					if shuffle:
						indices = shuffle_array[i1:i2]
						to_array[indices] = values
					else:
						to_array[i1:i2] = values

				progress_value += i2-i1
				if not progress(progress_value/float(progress_total)):
					break
			if shuffle: # write to disk in one go
				to_array_disk[:] = to_array

def export_fits(dataset, path, column_names=None, shuffle=False, selection=False, progress=None, virtual=True):
	"""
	:param DatasetLocal dataset: dataset to export
	:param str path: path for file
	:param lis[str] column_names: list of column names to export or None for all columns
	:param bool shuffle: export rows in random order
	:param bool selection: export selection or not
	:param progress: progress callback that gets a progress fraction as argument and should return True to continue,
		or a default progress bar when progress=True
	:param: bool virtual: When True, export virtual columns
	:return:
	"""
	if shuffle:
		random_index_name = "random_index"
		while random_index_name in dataset.get_column_names():
			random_index_name += "_new"

	column_names = column_names or (dataset.get_column_names() + (list(dataset.virtual_columns.keys()) if virtual else []))
	N = len(dataset) if not selection else dataset.selected_length()
	data_types = []
	data_shapes = []
	for column_name in column_names:
		if column_name in dataset.get_column_names():
			column = dataset.columns[column_name]
			shape = (N,) + column.shape[1:]
			dtype = column.dtype
		else:
			dtype = np.float64().dtype
			shape = (N,)
		data_types.append(dtype)
		data_shapes.append(shape)

	if shuffle:
		column_names.append(random_index_name)
		data_types.append(np.int64().dtype)
		data_shapes.append((N,))
	else:
		random_index_name = None

	vaex.file.colfits.empty(path, N, column_names, data_types, data_shapes)
	if shuffle:
		del column_names[-1]
		del data_types[-1]
		del data_shapes[-1]
	dataset_output = vaex.dataset.FitsBinTable(path, write=True)
	_export(dataset_input=dataset, dataset_output=dataset_output, path=path, random_index_column=random_index_name,
			column_names=column_names, selection=selection, shuffle=shuffle,
			progress=progress)

def export_hdf5(dataset, path, column_names=None, byteorder="=", shuffle=False, selection=False, progress=None, virtual=True):
	"""
	:param DatasetLocal dataset: dataset to export
	:param str path: path for file
	:param lis[str] column_names: list of column names to export or None for all columns
	:param str byteorder: = for native, < for little endian and > for big endian
	:param bool shuffle: export rows in random order
	:param bool selection: export selection or not
	:param progress: progress callback that gets a progress fraction as argument and should return True to continue,
		or a default progress bar when progress=True
	:param: bool virtual: When True, export virtual columns
	:return:
	"""

	if selection:
		assert dataset.has_selection(), "cannot export selection is there is none"
	# first open file using h5py api
	with h5py.File(path, "w") as h5file_output:

		h5data_output = h5file_output.require_group("data")
		#i1, i2 = dataset.current_slice
		N = len(dataset) if not selection else dataset.selected_length()
		if N == 0:
			raise ValueError("Cannot export empty table")
		logger.debug("exporting %d rows to file %s" % (N, path))
		column_names = column_names or (dataset.get_column_names() + (list(dataset.virtual_columns.keys()) if virtual else []))
		logger.debug(" exporting columns: %r" % column_names)
		for column_name in column_names:
			if column_name in dataset.get_column_names():
				column = dataset.columns[column_name]
				shape = (N,) + column.shape[1:]
				dtype = column.dtype
			else:
				dtype = np.float64().dtype
				shape = (N,)
			array = h5file_output.require_dataset("/data/%s" % column_name, shape=shape, dtype=dtype.newbyteorder(byteorder))
			array[0] = array[0] # make sure the array really exists
		random_index_name = None
		if shuffle:
			random_index_name = "random_index"
			while random_index_name in dataset.get_column_names():
				random_index_name += "_new"
			shuffle_array = h5file_output.require_dataset("/data/" + random_index_name, shape=(N,), dtype=byteorder+"i8")
			shuffle_array[0] = shuffle_array[0]

	# after this the file is closed,, and reopen it using out class
	dataset_output = vaex.dataset.Hdf5MemoryMapped(path, write=True)

	_export(dataset_input=dataset, dataset_output=dataset_output, path=path, random_index_column=random_index_name,
			column_names=column_names, selection=selection, shuffle=shuffle, byteorder=byteorder,
			progress=progress)
	return


if __name__ == "__main__":
	main(sys.argv)

def main(argv):
	import argparse
	parser = argparse.ArgumentParser(argv[0])
	parser.add_argument('--verbose', '-v', action='count', default=0)
	parser.add_argument('--list', '-l', default=False, action='store_true', help="list columns of input")
	parser.add_argument('--progress', help="show progress (default: %(default)s)", default=True, action='store_true')
	parser.add_argument('--no-progress', dest="progress", action='store_false')
	parser.add_argument('--shuffle', "-s", dest="shuffle", action='store_true', default=False)

	subparsers = parser.add_subparsers(help='type of input source', dest="task")

	parser_soneira = subparsers.add_parser('soneira', help='create soneira peebles dataset')
	parser_soneira.add_argument('output', help='output file')
	parser_soneira.add_argument("columns", help="list of columns to export (or all when empty)", nargs="*")
	parser_soneira.add_argument('--dimension','-d', type=int, help='dimensions', default=4)
	#parser_soneira.add_argument('--eta','-e', type=int, help='dimensions', default=3)
	parser_soneira.add_argument('--max-level','-m', type=int, help='dimensions', default=28)
	parser_soneira.add_argument('--lambdas','-l', type=int, help='lambda values for fractal', default=[1.1, 1.3, 1.6, 2.])


	parser_tap = subparsers.add_parser('tap', help='use TAP (Table Access Protocol) as source')
	parser_tap.add_argument("tap_url", help="input source or file")
	parser_tap.add_argument("table_name", help="input source or file")
	parser_tap.add_argument("output", help="output file (ends in .fits or .hdf5)")
	parser_tap.add_argument("columns", help="list of columns to export (or all when empty)", nargs="*")

	parser_file = subparsers.add_parser('file', help='use a file as source (e.g. .hdf5, .fits, .vot (VO table), .asc (ascii)')
	parser_file.add_argument("input", help="input source or file")
	parser_file.add_argument("output", help="output file (ends in .fits or .hdf5)")
	parser_file.add_argument("columns", help="list of columns to export (or all when empty)", nargs="*")

	args = parser.parse_args(argv[1:])

	verbosity = ["ERROR", "WARNING", "INFO", "DEBUG"]
	logging.getLogger("vaex").setLevel(verbosity[min(3, args.verbose)])

	if args.task == "soneira":
		if vaex.utils.check_memory_usage(4*8*2**args.max_level, vaex.utils.confirm_on_console):
			print("generating soneira peebles dataset...")
			dataset = vaex.dataset.SoneiraPeebles(args.dimension, 2, args.max_level, args.lambdas)
		else:
			sys.exit(1)
	if args.task == "tap":
		dataset = vaex.dataset.DatasetTap(args.tap_url, args.table_name)
		print("exporting from {tap_url} table name {table_name} to {output}".format(tap_url=args.tap_url, table_name=args.table_name, output=args.output))
	if args.task == "file":
		dataset = vaex.open(args.input)
		print("exporting from {input} to {output}".format(input=args.input, output=args.output))

	if dataset is None:
		print("Cannot open input")
		sys.exit(1)
	if args.list:
		print("columns names: " + " ".join(dataset.get_column_names()))
	else:
		if args.columns:
			columns = args.columns
		else:
			columns = None
		if columns is None:
			columns = dataset.get_column_names()
		for column in columns:
			if column not in dataset.get_column_names():
				print("column %r does not exist, run with --list or -l to list all columns")
				sys.exit(1)

		base, output_ext = os.path.splitext(args.output)
		if output_ext not in [".hdf5", ".fits"]:
			print("extension %s not supported, only .fits and .hdf5 are" % output_ext)
			sys.exit(1)

		print("exporting %d rows and %d columns" % (len(dataset), len(columns)))
		print("columns: " +" ".join(columns))
		with vaex.utils.progressbar("exporting") as progressbar:
			def update(p):
				progressbar.update(p)
				return True
			if output_ext == ".hdf5":
				export_hdf5(dataset, args.output, column_names=columns, progress=update, shuffle=args.shuffle)
			elif output_ext == ".fits":
				export_fits(dataset, args.output, column_names=columns, progress=update, shuffle=args.shuffle)
		print("\noutput to %s" % os.path.abspath(args.output))

