#!/usr/bin/python3
import argparse
import collections
import csv
import os
import re
import struct
import subprocess
import sys
import tempfile
import time

workdir = None
def read_memory(proc, address, length):
	global workdir
	if workdir is None:
		workdir = tempfile.TemporaryDirectory(prefix='tracebuffer.')
	length = length // 4 + (1 if length % 4 else 0)
	with tempfile.NamedTemporaryFile(dir=workdir.name) as fd:
		proc.stdin.write('mrd -bin -file {f:s} 0x{a:08x} 0x{l:x}\n'.format(f=fd.name, a=address, l=length).encode('utf8'))
		proc.stdin.flush()
		while os.stat(fd.name).st_size != length * 4:
			time.sleep(0.001)
		return bytearray(fd.read())

class LazyBufferInterface(object):
	def __init__(self, readpage, pagesize=0x10000, initial_buffer=bytearray()):
		self.readpage = readpage
		self.pagesize = pagesize
		self.buffer = initial_buffer
		self.pagemap = bytearray(b'\x01'*(len(initial_buffer)//pagesize) + (b'\x01' if len(initial_buffer) % pagesize else b''))
	def ensure_page(self, offset):
		pgaddr = offset - (offset % self.pagesize)
		pgnum  = offset // self.pagesize
		if len(self.pagemap) <= pgnum:
			self.pagemap += bytearray(pgnum - len(self.pagemap) + 1)
		if self.pagemap[pgnum] == 0:
			# We need to load this part of the buffer
			print('Loading {:>10s}'.format(hex(pgaddr)))
			if len(self.buffer) < pgaddr + self.pagesize:
				# Extend the buffer if needed.
				self.buffer += bytearray(pgaddr + self.pagesize - len(self.buffer))
			data = self.readpage(pgaddr)
			self.buffer[pgaddr:pgaddr+len(data)] = data
			self.pagemap[pgnum] = 1
	def __getitem__(self, key):
		if isinstance(key, tuple) and len(key) == 2 and isinstance(key[0], int) and isinstance(key[1], struct.Struct):
			off, st = key
			for i in range(off, off+st.size, self.pagesize):
				self.ensure_page(i) # ensure_page() on some address in every relevant page
			return st.unpack_from(self.buffer, off)
		if isinstance(key, slice):
			for i in range(key.start, key.stop, self.pagesize):
				self.ensure_page(i) # ensure_page() on some address in every relevant page
		else:
			self.ensure_page(key)
		return self.buffer[key]

class NamedStruct(struct.Struct):
	def __init__(self, *args, Names=None, **kwargs):
		if Names is None:
			self.__Named = tuple
		else:
			self.__Named = collections.namedtuple('NamedStructTuple', Names)
		super().__init__(*args, **kwargs)
	def unpack(self, *args, **kwargs):
		return self.__Named(*super().unpack(*args, **kwargs))
	def unpack_from(self, *args, **kwargs):
		return self.__Named(*super().unpack_from(*args, **kwargs))
	def iter_unpack(self, *args, **kwargs):
		return map(lambda x: self.__Named(*x), super().iter_unpack(*args, **kwargs))


parser = argparse.ArgumentParser()
parser.add_argument('-H','--host', help='The host to connect to', default='localhost')
parser.add_argument('-t','--target', help='The debug target to connect to when using the JTAG access method', type=int, default=2)
parser.add_argument('-a','--address', help='The start address of the buffer (int/hex) or ELF filename when using the JTAG access method', default=None)
parser.add_argument('-f','--file', help='A binary memory dump of the trace buffer', default=None)
parser.add_argument('--dump-script', help='Instead of reading a trace buffer, specify the size of the trace buffer to print an xsdb script to create a file dump.', type=lambda x: int(x,0), default=0)
ARGS = parser.parse_args()
del parser

BUF_HDR_FMT = NamedStruct('<2L', Names=('length','last_record'))

if ARGS.address is not None:
	try:
		ARGS.address = int(ARGS.address,0)
	except ValueError:
		ARGS.address = subprocess.check_output(['arm-none-eabi-nm', '-C', ARGS.address]).decode('utf8')
		ARGS.address = int(re.search('(?:^|\n)([0-9a-f]+) b tracebuffer_contents(?:\n|$)', ARGS.address).group(1),16)


if ARGS.dump_script:
	if not ARGS.file:
		ARGS.file = 'tracedump.bin'
	print('connect -host {}'.format(ARGS.host))
	print('target {}'.format(ARGS.target))
	print('stop')
	print('mrd -bin -file "{file}" 0x{addr:08x} 0x{len:x}'.format(file=ARGS.file, addr=ARGS.address, len=ARGS.dump_script))
	sys.exit(0)

elif ARGS.address:
	# JTAG path
	
	proc = subprocess.Popen('xsdb', stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, shell=False)

	print('Connecting to target.')
	proc.stdin.write('connect -host {}\n'.format(ARGS.host).encode('utf8'))
	proc.stdin.write('target {}\n'.format(ARGS.target).encode('utf8'))
	proc.stdin.write('stop\n'.encode('utf8'))
	proc.stdin.flush()
	time.sleep(0.5)

	BUF_START = ARGS.address

	print('Reading TraceBuffer headers.')
	buffer_header = BUF_HDR_FMT.unpack(read_memory(proc, BUF_START, BUF_HDR_FMT.size))
	BUFFER = LazyBufferInterface(lambda x: read_memory(proc, BUF_START+BUF_HDR_FMT.size+x, 0x10000), 0x10000)

elif ARGS.file:
	# File path
	#
	print('Reading file.')
	filedata = bytearray(open(ARGS.file,'rb').read())
	buffer_header = BUF_HDR_FMT.unpack(filedata[0:BUF_HDR_FMT.size])
	def error(addr):
		raise IndexError('Attempted to access buffer memory out of range: 0x{:x}'.format(addr))
	BUFFER = LazyBufferInterface(None, initial_buffer=filedata[BUF_HDR_FMT.size:])


print('Buffer size: {}'.format(buffer_header.length))
print('Last record: 0x{:x}'.format(buffer_header.last_record))
print()

if buffer_header.last_record == 0xffffffff:
	print('The buffer is empty.')
	sys.exit(0)

print('Parsing buffer...')
REC_HDR_FMT = NamedStruct('<LLLHHQ', Names=('prev_offset','label_len','data_len','loglevel','flags','timestamp'))
REC_HDR_LEN = REC_HDR_FMT.size
Record = collections.namedtuple('Record', ('offset','prev_offset','length','label','loglevel','flags','timestamp','data'))
def parse_rec(BUFFER, off):
	record_header = BUFFER[(off, REC_HDR_FMT)]
	rec_label = BUFFER[ off+REC_HDR_LEN : off+REC_HDR_LEN+record_header.label_len ].decode('utf8', errors='replace')
	rec_data  = BUFFER[ off+REC_HDR_LEN+record_header.label_len : off+REC_HDR_LEN+record_header.label_len+record_header.data_len ]
	if not (record_header.flags & 1):
		# Data is not binary:
		rec_data = rec_data.decode('utf8', errors='replace')
	return Record(
			off,
			record_header.prev_offset,
			REC_HDR_LEN+record_header.label_len+record_header.data_len,
			rec_label,
			record_header.loglevel,
			record_header.flags,
			record_header.timestamp,
			rec_data
			)

off = buffer_header.last_record
after_last_rec = None
RECORDS = []
wrapped = False # An assert helper in case of corruption.
while off != 0xffffffff:
	assert off < buffer_header.length
	rec = parse_rec(BUFFER, off)
	RECORDS.append(rec)
	if after_last_rec is None:
		after_last_rec = off + rec.length
	old_off = off
	off = rec.prev_offset
	assert off != old_off, "Infinite loop detected: previous record is this record?!"
	if off > old_off:
		assert not wrapped, "We've wrapped multiple times."
		wrapped = True
	if wrapped and off < after_last_rec:
		break # We're done.

RECORDS.reverse()
max_label_len = max(map(lambda x: len(x.label), RECORDS))
max_timestamp_len = max(map(lambda x: len(str(x.timestamp)), RECORDS))
loglevels = ["SILENT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DIAGNOSTIC", "TRACE", "ALL", "INHERIT"]
loglevels = dict(zip(range(0,len(loglevels)),map(lambda x: x[0:4], loglevels)))
fmt = '{{rec.timestamp:{max_timestamp_len}d}} | {{rec.label:<{max_label_len}s}} | {{loglevel:<4s}} | {{data}}'.format(max_label_len=max_label_len, max_timestamp_len=max_timestamp_len)

for rec in RECORDS:
	if isinstance(rec.data, str):
		msg = rec.data.rstrip('\r\n')
	elif isinstance(rec.data, bytearray):
		msg = 'BIN: ' + ' '.join(map(lambda x: '{:02x}'.format(x), rec.data))
	else:
		print(type(rec.data))
		assert False, 'Unexpected record data type'
	print(fmt.format(rec=rec, data=msg, loglevel=loglevels.get(rec.loglevel,str(rec.loglevel))))
