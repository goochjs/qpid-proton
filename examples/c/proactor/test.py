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

# This is a test script to run the examples and verify that they behave as expected.

import unittest, sys, time
from exampletest import *

def python_cmd(name):
    dir = os.path.dirname(__file__)
    return [sys.executable, os.path.join(dir, "..", "..", "python", name)]

def receive_expect(n):
    return ''.join('{"sequence"=%s}\n'%i for i in xrange(1, n+1)) + "%s messages received\n"%n

class CExampleTest(BrokerTestCase):
    broker_exe = ["broker"]

    def test_send_receive(self):
        """Send first then receive"""
        s = self.proc(["send", "-a", self.addr])
        self.assertEqual("100 messages sent and acknowledged\n", s.wait_out())
        r = self.proc(["receive", "-a", self.addr])
        self.assertEqual(receive_expect(100), r.wait_out())

    def test_receive_send(self):
        """Start receiving  first, then send."""
        r = self.proc(["receive", "-a", self.addr]);
        s = self.proc(["send", "-a", self.addr]);
        self.assertEqual("100 messages sent and acknowledged\n", s.wait_out())
        self.assertEqual(receive_expect(100), r.wait_out())

    def test_timed_send(self):
        """Send with timed delay"""
        s = self.proc(["send", "-a", self.addr, "-d100", "-m3"])
        self.assertEqual("3 messages sent and acknowledged\n", s.wait_out())
        r = self.proc(["receive", "-a", self.addr, "-m3"])
        self.assertEqual(receive_expect(3), r.wait_out())

    def test_send_direct(self):
        """Send to direct server"""
        with bind0() as sock:
            addr = "127.0.0.1:%s/examples" % sock.port()
            d = self.proc(["direct", "-a", addr])
            d.wait_re("listening")
            self.assertEqual("100 messages sent and acknowledged\n", self.proc(["send", "-a", addr]).wait_out())
            self.assertIn(receive_expect(100), d.wait_out())

    def test_receive_direct(self):
        """Receive from direct server"""
        with bind0() as sock:
            addr = "127.0.0.1:%s/examples" % sock.port()
            d = self.proc(["direct", "-a", addr])
            d.wait_re("listenin")
            self.assertEqual(receive_expect(100), self.proc(["receive", "-a", addr]).wait_out())
            self.assertIn("100 messages sent and acknowledged\n", d.wait_out())


if __name__ == "__main__":
    unittest.main()
