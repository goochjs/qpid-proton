#ifndef PROTON_CONTAINER_H
#define PROTON_CONTAINER_H

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

/**@file
 * Container API for the proton engine.
 * @defgroup container Container
 * A container for AMQP connections.
 *
 * The container manages multiple connections and dispatches events to handler
 * functions in one or more threads. If you call pn_container_run() in a single
 * thread and don't have any other threads then your application is single
 * threaded.
 *
 * In a multi-threaded application, most pn_container_() functions are thread
 * safe but always read the *Thread Safety* notes. The @ref engine functions
 * and pn_incref(), pn_decref() are *not* thread safe when operating on values
 * belonging to a single connection. They are safe to call concurrently on
 * values belonging to separate connections. pn_container_inject() allows you
 * to safely "inject" a function call into the proper event-loop thread for a
 * connection.
 *
 * @{
 */

#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// FIXME aconway 2016-08-16: thread safety review & doc.
// use incref in handlers or before connnect() to pin connections.

// FIXME: This API designed to be as simple as possible to implement while supporting
// all the needs of language binding containers like C++, Python, Ruby, Go.
// Extra conveniences can be added in the bindings, or later in C if we find they
// are useful in many languages.

// Forward declarations
typedef struct pn_connection_t pn_connection_t;
typedef struct pn_event_t pn_event_t;

/// The container struct. Not a reference-counted object, do not use pn_incref()/pn_decref().
typedef struct pn_container_t pn_container_t;

/// Create a default container. Must be freed with pn_container_free.
/// Not a reference-counted object, do not use pn_incref()/pn_decref().
PN_EXTERN pn_container_t* pn_container(const char* id);

/// Free the container.
PN_EXTERN void pn_container_free(pn_container_t* c);

/// Get the container ID. The string is valid until the container is freed.
PN_EXTERN const char* pn_container_id(pn_container_t* c);

/// Handler function pointer, called by the container to handle an @ref event.
typedef void (*pn_handler_fn)(pn_event_t* event);

/// Connect to a remote address string (see @ref url). The returned connection
/// is not active, so you can safely configure settings before calling
/// pn_container_activate().
///
/// @return connection, belongs to the container. Connection errors errors will
/// be reported to the connection's handler when you call pn_container_activate().
PN_EXTERN pn_connection_t* pn_container_connect(pn_container_t* c, const char* url);

/// Call pn_connection_open() and activate the connection so that the container
/// will call handler to receive events related to the connection.
///
/// *Thread safety*: the connection's handler is active after this, use
/// pn_container_inject to inject operations from another thread.
PN_EXTERN void pn_container_activate(pn_container_t* c, pn_connection_t*, pn_handler_fn handler);

/// Accept function pointer, called when an incoming connection is accepted by
/// the container.
///
/// The accept function can reject the connection by setting
/// pn_connection_condition() and call pn_connection_close(), or accept it by
/// calling pn_container_activate().
///
/// @param c the connection, NULL means listener is closed, there will be no more calls.
/// @param error message if listener closed due to error, NULL otherwise.
/// @param context pointer provided by the application in listen.
typedef void (*pn_accept_fn)(pn_connection_t* c, const char* error, void* context);

/// Opaque identifier for a stream of incoming connections started by pn_listen.
typedef struct pn_listener_t pn_listener_t;

/// Listen on URL, call accept(connection, context) for each incoming connection.
PN_EXTERN pn_listener_t* pn_container_listen(
    pn_container_t* c, const char* url, pn_accept_fn accept, void* context);

/// Stop listening
PN_EXTERN void pn_container_stop_listening(pn_container_t* c, pn_listener_t* listener);

#define PN_FOREVER -1

/// work() does some container-defined unit of work then returns.
///
/// *Thread Safety*: May be called in multiple threads.
///
/// timeout==0 means return immediately if there is no work to do.
/// timeout>0 means return after timeout ms if there's no work.
/// timeout==PN_FOREVER means return only if pn_container_interrupt is called.
///
/// @return
/// - PN_OK: Some work was done.
/// - PN_TIMEOUT: the timeout expired with no work done.
/// - PN_INTR: pn_container_interrupt was called.
/// - PN_EOS: pn_container_stop was called.
///
PN_EXTERN int pn_container_work(pn_container_t* c, pn_nanoseconds_t timeout);

/// Run till container is stopped.
/// Equivalent to `while (pn_container_work(c, PN_FOREVER) != PN_EOS);`
PN_EXTERN void pn_container_run(pn_container_t* c);

/// Make work() return immediately in all threads with PN_INTR
PN_EXTERN void pn_container_interrupt(pn_container_t* c);


/// Mark the container as stopped, abort all connections, clean up all connection memory.
/// pn_container_work() returns in all threads with PN_EOS, future calls to work() return PN_EOS immediately.
PN_EXTERN void pn_container_stop(pn_container_t* c);

/// A simple callback function with void* context.
typedef void (*pn_callback_fn)(void* context);

/// Arrange for callback(context) to be executed after a delay
///
/// *Thread safety*: may be executed concurrently.
PN_EXTERN void pn_container_schedule(
    pn_container_t* c, pn_nanoseconds_t, pn_callback_fn callback, void* context);

/// Arrange for callback(context) to be executed as soon as possible in the event-loop
/// associated with connection.
///
/// *Thread safety*: may be executed concurrently.  Will be serialized with
/// calls to the event handler for that connection.
PN_EXTERN void pn_container_inject(
    pn_container_t* c, pn_connection_t* connection, pn_callback_fn callback, void* context) ;

/// Get the container associated with a connection.
PN_EXTERN pn_container_t *pn_connection_container(pn_connection_t*);

/// Get the container associated with an event.
PN_EXTERN pn_container_t *pn_event_container(pn_event_t *event);

///@}

#ifdef __cplusplus
}
#endif
#endif // PROTON_CONTAINER_H
