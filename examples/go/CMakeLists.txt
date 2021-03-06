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

if(BUILD_GO)

  set(examples electron/broker electron/receive electron/send proton/broker)

  # Build example exes
  foreach(example ${examples})
    string(REPLACE / - target ${example})
    set(target "go-example-${target}")
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${example})
    # Always run go_build, it will do nothing if there is nothing to do.
    # Otherwise it's too hard to get the dependencies right.
    add_custom_target(${target} ALL
      COMMAND ${GO_BUILD} ${GO_EXAMPLE_FLAGS} -o ${output} ${CMAKE_CURRENT_SOURCE_DIR}/${example}.go
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS go-build)
  endforeach()

  # Build test driver exe
  set(test_exe ${CMAKE_CURRENT_BINARY_DIR}/example_test)
  add_custom_target(go-example-test ALL
    COMMAND ${GO_TEST} -c -o ${test_exe} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.go
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  add_test(
    NAME go-example-electron
    COMMAND ${GO_ENV} ${test_exe} -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker broker)

  add_test(
    NAME go-example-proton
    COMMAND ${GO_ENV} ${test_exe} -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker ../proton/broker)

  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES ${examples})
endif()
