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
 *
 * Container API for the proton engine.
 *
 * @defgroup container Container
 * A container for AMQP connections.
 *
 * The container manages IO for multiple connections and dispatches pn_event_t
 * events to threads that call pn_container_wait(). It can operate in single or
 * multi-threaded mode. See pn_container_wait() for details.
 *
 * The container can make connections as a client and/or listen for connections
 * as a server. Container functions are asynchronous: they do not return error
 * values but cause events to be dispatched which may contain error values.
 *
 * The container API is designed to be usable in C, but is as simple as possible
 * to make multiple implementations easier. It does not use any call-back
 * functions to avoid difficulties with language binding tools like SWIG, CGO or
 * CFFI, as well as deadlock and other re-entrancy problems in multi-threaded
 * code.
 *
 * External libraries can provide alternate container implementations which may
 * or may not support multi-threading. See @io_integration.
 *
 * @{
 */

#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pn_connection_t pn_connection_t;
typedef struct pn_event_t pn_event_t;

/**
 * The container struct.
 *
 * Use pn_container_free() to free, this is *not* a reference-counted object, do
 * *not* use pn_incref()/pn_decref().
 */
typedef struct pn_container_t pn_container_t;

/**
 * Create a container, free with pn_container_free().
 * pn_container_t is NOT a reference-counted object, do not use pn_incref()/pn_decref().
 */
PN_EXTERN pn_container_t *pn_container(const char *id);

/** Free the container. */
PN_EXTERN void pn_container_free(pn_container_t *c);

/**
 * Create a client connection to a remote address string (see @ref url).
 *
 * Dispatch a PN_CONNECTION_INIT event with the un-opened connection.
 *
 * When the event is returned by pn_connection_wait(), check for connect errors
 * on the pn_connection_condition(). If there are none, configure the connection
 * and call pn_connection_open() to start the AMQP protocol.
 *
 * @param context passed with the PN_CONNECTION_INIT event as pn_event_context()
 */
PN_EXTERN void pn_container_connect(pn_container_t *c, const char *url, void *context);

/**
 * Listen for incoming connections on URL.
 *
 * When a connection is accepted, dispatch a PN_CONNECTION_INIT event
 * containing the un-opened connection.
 *
 * When the event is retunred by pn_connection_wait() you can reject the
 * connection by setting an error on pn_connection_condition(). Otherwise event
 * dispatch for the connection will continue with PN_CONNECTION_REMOTE_OPEN etc.
 *
 * If a listener fails a PN_CONTAINER_LISTENER_ERROR event with the context
 * will be dispatched.
 *
 * @param context passed with the PN_CONNECTION_INIT event as pn_event_context()
 */
PN_EXTERN void pn_container_listen(pn_container_t *c, const char *url, void *context);

// FIXME aconway 2016-09-01: Need a place to attach error condition for
// PN_CONTAINER_LISTENER_ERROR. There's no connection or transport and the
// container is shared. Would be nice to attach to the event itself?

// FIXME aconway 2016-09-01: review/document incoming events for security.
// We start at CONN_INIT since that gives us visibility & influence on the entire lifecycle.
// but we may want to document use of INIT, BOUND, OPEN, TRANSPORT_AUTHENTICATED, others?
//
// Can we keep all the connection central and avoid transport events or do we
// need need to look at the transport before it has a connection?

/**
 * Close the listener(s) identified by the context.
 *
 * Dispatch a PN_CONTAINER_LISTENER_CLOSED event for each listener closed.
 *
 * @param context identifies listener(s) to close and is passed to pn_event_context()
 */
PN_EXTERN void pn_container_close_listener(pn_container_t *c, void *context);

/**
 * Close all open listeners.
 *
 * Dispatche a PN_CONTAINER_LISTENER_CLOSED event for each listener, with the listener's context.
 *
 * @param context identifies listener(s) to close.
 */
PN_EXTERN void pn_container_close_listeners(pn_container_t *c);

/** Timeout value indicating wait forever */
#define PN_FOREVER -1

/**
 * Wait for a dispatched event to be available.
 *
 * Once the event is processed, you *must* call pn_event_done(event)
 *
 * A single-threaded application calls pn_container_wait() in a loop until
 * it returns a PN_CONTAINER_STOPPED event.
 *
 * *Thread Safety*: A multi-threaded application can call pn_container_wait() in
 * multiple threads to create a thread pool. Different connections can be
 * processed concurrently in different threads.
 *
 * Note however that the @ref engine API types and pn_incref()/pn_decref() are
 * *not* thread safe for values associated with a *single* connection.
 * pn_container_wait() ensures that events for a single connection are never
 * returned concurrently so you can safely use the returned event without
 * locking. You must call pn_event_done() when you are finished so further
 * events for the same connection can be dispatched.
 *
 * Sometimes threads other than the pn_container_wait() thread need to affect a
 * connection. pn_container_inject() injects a custom event that is dispatched
 * to a pn_container_wait() thread where the connection can be used safely.
 */
PN_EXTERN pn_event_t *pn_container_wait(pn_container_t *c, pn_nanoseconds_t timeout);


/**
 * Indicate that we are done processing this event.
 */
PN_EXTERN pn_event_t *pn_event_done(pn_event_t *e);

/* FIXME aconway 2016-09-01: move to event.h? */

/**
 * Interrupt one pn_container_wait() thread.
 *
 * Returns PN_CONTAINER_INTERRUPT event immediately in *at most one* thread
 * blocked in pn_container_wait().  If no threads are currently blocked, this
 * does nothing. Allows shrinking the thread pool as well as growing it.
 *
 * *Thread safe*
 *
 * @param context passed with the PN_CONTAINER_INTERRUPT event as pn_event_context()
 */
PN_EXTERN void pn_container_interrupt(pn_container_t *c, void *context);


/**
 * Schedule a timed event.
 *
 * Dispatch a PN_CONTAINER_TIMER event after delay.
 *
 * *Thread safe*
 *
 * @param context passed with the PN_CONNECTION_TIMER event as pn_event_context()
 */
PN_EXTERN void pn_container_schedule(pn_container_t *c, pn_nanoseconds_t delay, void *context);

/**
 * Inject a custom event.
 *
 * Dispatch a PN_CONTAINER_INJECT event, serialized with other events for the
 * connection.  When pn_container_wait() returns the event, it is safe to use
 * the connection as no other thread is using it. You must call pn_event_done()
 * when you are finished to allow dispatch to continue for that connection.
 *
 * *Thread safe*.
 *
 * @param context passed with the PN_CONNECTION_STOPPED event as pn_event_context()
 */
PN_EXTERN void pn_container_inject(pn_container_t *c, pn_connection_t *connection, void *context) ;

/**
 * Equivalent to calling pn_container_inject() for all currently existing connections.
 * Can be used to implement global shut-down or similar operations.
 *
 * Note: if there are still open listeners, or threads calling pn_container_connect() then
 * connections created concurrently or after this call may not be included.
 */
PN_EXTERN void pn_container_inject_all(pn_container_t *c, void *context) ;

/*
 * FIXME aconway 2016-09-01: note there is no stop(). It can be implemented with
 * close_listeners, inject_all and interrupt, with any desired semantics
 * (abrupt, send close(), wait for close() (up to timeout) etc.) The desired
 * semantics of stop() are still to vague to pin down in C but we can implement
 * the semantics of existing containers (C++, python etc.) using these tools.
 */

/* Get the container ID. The string is valid until the container is freed. */
PN_EXTERN const char *pn_container_id(pn_container_t *c);

/// Get the container associated with a connection.
PN_EXTERN pn_container_t *pn_connection_container(pn_connection_t*);

/// Get the container associated with an event.
PN_EXTERN pn_container_t *pn_event_container(pn_event_t *event);

///@}

#ifdef __cplusplus
}
#endif
#endif // PROTON_CONTAINER_H
