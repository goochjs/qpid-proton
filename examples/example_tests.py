#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License
#

"""Support library for automated example tests in python"""

import sys, os.path, subprocess, json, time, shutil
import platform

def retry(f, timeout=5, delay=0.1):
    ex = ""
    while delay < timeout:
        try:
            if f(): return
        except Exception, e:
            ex = e
        time.sleep(delay)
        delay *= 2
    raise AssertionError("timed out waiting for %s %s" % (f, ex))

def check_exe(env):
    exe = os.environ.get(env)
    if exe and os.path.isfile(exe) and os.access(exe, os.X_OK):
        return exe

devnull = open(os.devnull,"w")

class ProcError(Exception):
    """An exception that captures failed process output"""
    def __init__(self, cmd, retcode, out):
        if out:
            out = out.strip()
        if '\n' in out:         # Multi-line
            out = "\nvvvvvvvvvvvvvvvv\n%s\n^^^^^^^^^^^^^^^^\n" % out
        super(Exception, self, ).__init__(
            "%s exit-code=%s: %s" % (cmd, retcode, out))

def check_call(cmd):
    """Like subprocess check_call but include stdout/stderr in exception"""
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out, err) = p.communicate();
    print >>sys.stderr, "FIXME", cmd, err, out
    if p.returncode != 0:
        raise ProcError(cmd, p.returncode, out)

class Broker(object):
    def __init__(self, kind, directory, port):
        script = "%s_broker" % kind
        self.port = port
        self.name = name
        self.dir = os.path.abspath(self.name)

    def wait_alive(self):
        retry(self.alive)
