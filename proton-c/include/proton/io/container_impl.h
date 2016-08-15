#ifndef PROTON_CONTAINER_IMPL_H
#define PROTON_CONTAINER_IMPL_H

/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <proton/container.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @defgroup container_impl Container Implementation
/// @ingroup io_integration
/// @{
///

/// Implemntation of a container, see container.h for semantics.
///
/// The implementer should create their own implementation `struct` e.g.
///
///     struct my_container_impl { pn_container_t c; ... };
///
/// The first implementation `struct` member should be a pn_container_t struct
/// so that the pn_container_t* can be cast to my_container_impl* to access
/// extra members. All the pointers in the pn_container_t should be initialized
/// with appropriate functions and an id string.
///
/// The implementation must provide at least one public function that returns a
/// pn_container_t* pointing to a container initialized by the implementation.
///
typedef struct pn_container_t {
    void (*free)(pn_container_t* c);
    pn_connection_t* (*connect)(pn_container_t* c, const char* url);
    pn_listener_t* (*listen)(pn_container_t* c, const char* url, pn_accept_fn, pn_listener_t*);
    void (*stop_listening)(pn_container_t* c, pn_listener_t* listen_token);
    void (*activate)(pn_container_t* c, pn_connection_t*, pn_handler_fn handler);
    bool (*work)(pn_container_t* c, pn_nanoseconds_t timeout);
    void (*interrupt)(pn_container_t* c);
    void (*schedule)(pn_container_t* c, pn_nanoseconds_t, pn_callback_fn callback, void* context);
    void (*inject)(pn_container_t* c, pn_connection_t* connection, pn_callback_fn callback, void* context) ;

    const char *id;
} pn_container_t;

///@}

#ifdef __cplusplus
}
#endif

#endif // PROTON_CONTAINER_IMPL_H
