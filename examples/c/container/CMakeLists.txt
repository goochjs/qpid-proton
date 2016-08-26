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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${Proton_INCLUDE_DIRS})
add_definitions(${COMPILE_WARNING_FLAGS} ${WERROR} ${COMPILE_PLATFORM_FLAGS} ${LINK_TIME_OPTIMIZATION})

# Build the examples linked with ${prefix}_container.c
function(c_examples prefix includes libs)
  foreach(example simple_send)
    set_source_files_properties(${prefix}_container.c
      PROPERTIES INCLUDE_DIRS ${includes})
    if(BUILD_WITH_CXX)
      set_source_files_properties(${example}.c ${prefix}_container.c
        PROPERTIES LANGUAGE CXX)
    endif(BUILD_WITH_CXX)
    add_executable(${prefix}-${example} ${example}.c  ${prefix}_container.c)
    target_link_libraries(${prefix}-${example} ${Proton_LIBRARIES} ${libs})
  endforeach()

  if(WIN32)
    set(example_bin "$<TARGET_FILE_DIR:${prefix}-simple_send>")
    set(test_path "${example_bin};$<TARGET_FILE_DIR:qpid-proton>")
  else(WIN32)
    set(test_path "${example_bin}:$ENV{PATH}")
  endif(WIN32)
  set(run_env ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py)
  add_test(NAME "c-example-${prefix}"
    COMMAND ${run_env} -- "PATH=${test_path}" ${VALGRIND_ENV}
    ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v ${prefix})
endfunction()

find_package(Libuv)
if (Libuv_FOUND)
  c_examples(libuv ${Libuv_INCLUDE_DIRS} ${Libuv_LIBRARIES})
endif()
