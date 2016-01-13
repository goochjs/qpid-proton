#--
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
#++

module Qpid::Proton::Util
  # Mutable, thread-safe counter
  class Counter
    def initialize
      @lock = Mutex.new
      @count = 0 
    end

    def next
      @lock.synchronize { @count += 1 }
    end
  end

  # Immutable (frozen) String container-id that can generate unique link IDs
  # by combining the container-id with a thread-safe counter.
  class ContainerId < String

    # id must be convertible to String. Generate a random UUID if id is not supplied.
    def initialize id=nil
      super(id || SecureRandom.uuid)
      @count  = Counter.new
      freeze
    end

    # Generate a unique id of the form '<hex-digits> + "@" + self'.
    def next_id
      @count.next.to_s(16) + "@" + self
    end
  end
end
