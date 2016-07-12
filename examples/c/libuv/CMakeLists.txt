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

find_package(Proton REQUIRED)
find_package(Libuv)

if (Libuv_FOUND)

  include_directories(${Proton_INCLUDE_DIRS} ${Libuv_INCLUDE_DIRS})

  foreach(example
      simple_send)
    set_source_files_properties(
      ${example}.c
      PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${WERROR} ${COMPILE_PLATFORM_FLAGS} ${LINK_TIME_OPTIMIZATION}")
    if(BUILD_WITH_CXX)
      set_source_files_properties(${example}.c PROPERTIES LANGUAGE CXX)
    endif(BUILD_WITH_CXX)
    add_executable(libuv-${example} ${example}.c)
    set_target_properties(libuv-${example} PROPERTIES OUTPUT_NAME ${example})
    target_link_libraries(libuv-${example} ${Proton_LIBRARIES} ${Libuv_LIBRARIES})
  endforeach()

endif()
