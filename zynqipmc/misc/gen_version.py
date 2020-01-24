#!/usr/bin/python3
import json
import os
import pwd
import re
import socket
import subprocess
import sys

version = {}

version['git']             = {}
version['git']['hash']     = subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode('utf8').strip()
version['git']['short']    = version['git']['hash'][:8]
version['git']['uint32_t'] = int(version['git']['short'],16)
version['git']['describe'] = subprocess.check_output(['git', 'describe', '--always', '--match=*-v[0-9]*.[0-9]*.[0-9]*', '--dirty']).decode('utf8').strip()
version['git']['dirty']    = version['git']['describe'].endswith('-dirty')

describe_long = subprocess.check_output(['git', 'describe', '--always', '--match=*-v[0-9]*.[0-9]*.[0-9]*', '--dirty', '--long']).decode('utf8').strip()
m = re.match(r'^(?P<tag>.*)-v(?P<version>(?:(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<revision>0|[1-9]\d*)(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?))-(?P<plus_commits>[0-9]+)-g[0-9a-f]{7}(?P<dirty>-dirty)?$', describe_long)
if m is None:
	version['version'] = None
else:
	version['version'] = m.groupdict()
	for i in ('major', 'minor', 'revision', 'plus_commits'):
		version['version'][i] = int(version['version'][i])
	version['version']['dirty'] = bool(version['version']['dirty'])
	for i in ('prerelease','buildmetadata'):
		if version['version'][i] is None:
			version['version'][i] = ""

version['build']                  = {}
version['build']['human_date']    = subprocess.check_output(['date']).decode('utf8').strip()
version['build']['machine_date']  = subprocess.check_output(['date', '-d', version['build']['human_date'], '+%Y-%m-%dT%H:%M:%S%z']).decode('utf8').strip()
version['build']['user']          = pwd.getpwuid(os.getuid())[0]
version['build']['host']          = socket.getfqdn()
version['build']['configuration'] = os.environ.get('BUILD_CONFIGURATION','')

version['summary'] = 'Version {version[git][describe]} ({version[git][short]}), built by {version[build][user]}@{version[build][host]} at {version[build][human_date]}'.format(version=version)

print(json.dumps(version, sort_keys=True))
