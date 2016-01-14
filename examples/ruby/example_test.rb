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

def random_port
  return Random.new.rand(10000) + 10000
end

def wait_port port
  # Wait for something to be listening on a port.
  for i in 0..100
    begin
      TCPSocket.open("", port).close
      return
    rescue Errno::ECONNREFUSED
      sleep(0.1)
    end
  end
  raise "timeout waiting for port #{port}"
end

module ExampleTest

  def run_script(script, port)
    assert File.exist? script
    cmd = [RbConfig.ruby, script]
    cmd += ["-a", ":#{port}/examples"] if port
    return IO.popen(cmd)
  end


  def assert_output(script, want, port=nil)
    out = run_script(script, port)
    assert_equal want.strip, out.read.strip
  end

  def test_helloworld
    assert_output("#{@dir}/helloworld.rb", "Hello world!", $broker_port)
  end

  def test_simple_send_recv
    assert_output("#{@dir}/simple_send.rb", "All 100 messages confirmed!", $broker_port)
    want = (0..99).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output("#{@dir}/simple_recv.rb", want, $broker_port)
  end

  def test_simple_send_direct_recv
    port = random_port
    recv = run_script("#{@dir}/direct_recv.rb", port)
    sleep(0.1)                  # FIXME aconway 2016-01-05: wait for ready.
    assert_output("#{@dir}/simple_send.rb", "All 100 messages confirmed!", port)
    Process.kill :TERM, recv.pid
    want = (0..99).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_equal want.strip, recv.read.strip
  end

  def test_direct_send_simple_recv
    port = random_port
    send = run_script("#{@dir}/direct_send.rb", port)
    sleep(0.1)                  # FIXME aconway 2016-01-05: wait for ready.
    want = (0..99).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output("#{@dir}/simple_recv.rb", want, port)
    Process.kill :TERM, send.pid
    assert_equal "All 100 messages confirmed!", send.read.strip
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
    srv = run_script("#{@dir}/server.rb", $broker_port)
    assert_output("#{@dir}/client.rb", want.strip, $broker_port)

  ensure
    Process.kill :TERM, srv.pid if srv
  end

end

def start_broker dir
  $broker_port = random_port
  $broker = spawn("#{RbConfig.ruby} #{dir}/broker.rb -a :#{$broker_port}")
  wait_port $broker_port
end

def stop_broker
  Process.kill :TERM, $broker
  Process.wait $broker
end


class ReactorTest < Test::Unit::TestCase
  DIR = "reactor"
  class << self
    def startup; start_broker DIR end
    def shutdown; stop_broker end
  end
  def setup; @dir = DIR end
  include ExampleTest
end

class EngineTest < Test::Unit::TestCase
  DIR = "engine"
  class << self
    def startup; start_broker DIR end
    def shutdown; stop_broker end
  end
  def setup; @dir = DIR end
  include ExampleTest
end
