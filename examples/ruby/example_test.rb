#!/use/bin/enc ruby
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
# under the License.
#

require 'test/unit'
require 'qpid_proton'
require 'socket'

$port = Random.new.rand(10000) + 10000

module ExampleTest

  def run_script(script, port)
    assert File.exist? script
    cmd = [RbConfig.ruby, script]
    cmd += ["-a", ":#{port}/examples"] if port
    return IO.popen(cmd)
  end


  def assert_output(script, want, port=nil)
    out = run_script(script, port)
    assert_equal want, out.read.strip
  end

  def test_helloworld
    assert_output("#{@dir}/helloworld.rb", "Hello world!", $port)
  end

  def test_send_recv
    assert_output("#{@dir}/simple_send.rb", "All 100 messages confirmed!", $port)
    want = (0..99).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output("#{@dir}/simple_recv.rb", want.strip, $port)
  end

  def test_client_server
    want =  <<EOS
-> Twas brillig, and the slithy toves
<- TWAS BRILLIG, AND THE SLITHY TOVES
-> Did gire and gymble in the wabe.
<- DID GIRE AND GYMBLE IN THE WABE.
-> All mimsy were the borogroves,
<- ALL MIMSY WERE THE BOROGROVES,
-> And the mome raths outgrabe.
<- AND THE MOME RATHS OUTGRABE.
EOS
    srv = run_script("#{@dir}/server.rb", $port)
    assert_output("#{@dir}/client.rb", want.strip, $port)

  ensure
    Process.kill :TERM, srv.pid if srv
  end
end

class ReactorTest < Test::Unit::TestCase
  def setup; @dir = "reactor" end
  include ExampleTest
end

class EngineTest < Test::Unit::TestCase
  def setup; @dir = "engine" end
  include ExampleTest
end

begin
  # FIXME aconway 2016-01-04: test both brokers
  broker = spawn("#{RbConfig.ruby} reactor/broker.rb -a :#{$port}")
  # Wait for the broker to be listening.
  for i in 0..10
    begin
      s = TCPSocket.open "", $port
      s.close
    rescue Errno::ECONNREFUSED
      sleep(0.1)
    end
  end
  Process.exit Test::Unit::AutoRunner.run
ensure
  if broker
    Process.kill :TERM, broker
    Process.wait broker
  end
end
