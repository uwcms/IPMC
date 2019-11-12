#!/usr/bin/python3
import argparse
import re
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(add_help=False)
parser.add_argument('--help', action='help', help='Show this help message and exit')
parser.add_argument('-h','--host', action='store', required=True, help="The hostname or IP address of the IPMC to connect to.")
parser.add_argument('-u','--user', action='store', default='ipmc', help="The username to log into the IPMC with.")
parser.add_argument('-p','--password', action='store', default='ipmc', help="The password to log into the IPMC with.")
parser.add_argument('upload_file', action='store', help="The file to upload to the IPMC.")
parser.add_argument('target_image', action='store', help="The upload target image for this file on the IPMC. (\"A\", \"B\", etc)")
ARGS = parser.parse_args()

tf = tempfile.TemporaryFile(mode='w+')

tf.write('''
user "{ARGS.user}" "{ARGS.password}"
bin
send {ARGS.upload_file} virtual/{ARGS.target_image}.bin
quit
'''.format(ARGS=ARGS))
tf.flush()
tf.seek(0,0)

proc = subprocess.Popen(['ftp', '-p', '-n', '-v', ARGS.host], stdin=tf, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
output = proc.stdout.read().decode('utf8').split('\n')
exitcode = 0
for line in output:
	if ' bytes sent in ' in line:
		print(line)
	if line.startswith('450 ') or ' bytes sent in ' in line:
		print(line[4:])
		exitcode = 1
sys.exit(exitcode)
