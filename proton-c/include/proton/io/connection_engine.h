#ifndef PROTON_CONNECTION_ENGINE_H
#define PROTON_CONNECTION_ENGINE_H

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

///@file
///
/// @defgroup connection_engine Connection Engine
/// @ingroup io_integration
/// @{
///
/// Internal SPI providing a bytes-in/bytes-out interface to connect any IO
/// source/destination to the Proton \ref engine API.  Consumes AMQP-encoded
/// input bytes and generates pn_event_t events for application code, then
/// gathers AMQP state information from the \ref engine objects to generate
/// AMQP-encoded output bytes.
///
/// pn_connection_engine_t instances are *not* thread safe but they are
/// independent, so separate instances can be used concurrently. For example a
/// multi-threaded application can process connections in multiple threads,
/// provided work on each connection is serialized.
///
/// Summary of use:
///
/// - while !pn_connection_engine_finished()
///   - Read data from your source into pn_connection_engine_read_buffer()
///   - Call pn_connection_engine_read_done() to indicate how much was read.
///   - Call pn_connection_engine_dispatch() to dispatch events until it returns NULL.
///   - Write data from pn_connection_engine_write_buffer() to your destination.
///   - Call pn_connection_engine_write_done() to indicate how much was written.
///
/// *Synchronous and Asynchronous IO*: The `_buffer()` and `_done()` functions
/// are separate so you can post an asynchronous read or write request with the
/// `_buffer()`, then later signal completion of that request with `_done()`.
///
/// *Error handling*: most `pn_connection_engine_` functions do not return an
/// error code. If an error occurs, the transport will be closed with an error
/// condition, the handler will receive a `PN_TRANSPORT_ERROR` event, and
/// pn_connection_engine_finished() will return `true` once all final processing
/// is complete. Thus the event handling application code is in change of error
/// handling, not the IO integration code.
///

#include <proton/condition.h>
#include <proton/event.h>
#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// A connection engine is a trio of pn_connection_t, pn_transport_t and pn_collector_t.
/// Use the pn_connection_engine_*() functions to operate on it.
/// It is a plain struct, not a proton object. Use pn_connection_engine_init to set up
/// the initial objects and pn_connection_engine_final to release them.
///
typedef struct pn_connection_engine_t {
    pn_connection_t* connection;
    pn_transport_t* transport;
    pn_collector_t* collector;
    pn_event_t* event;
} pn_connection_engine_t;

/// Initialize a pn_connection_engine_t struct with a new connection, transport
/// and collector. Return 0 on success, a proton error code on failure.
PN_EXTERN int pn_connection_engine_init(pn_connection_engine_t* engine);

/// Release the connection, transport and collector associated with engine, set all the pointers
/// to NULL. Only call on an engine that was initialized with pn_connection_engine_init
PN_EXTERN void pn_connection_engine_final(pn_connection_engine_t* engine);

/// The engine's read buffer. Read data from your IO source to buf.start, up to
/// a max of buf.size. Then call pn_connection_engine_read_done().
///
/// buf.size==0 means the engine cannot read presently, calling
/// pn_connection_engine_dispatch() may create more buffer space.
///
PN_EXTERN pn_rwbytes_t pn_connection_engine_read_buffer(pn_connection_engine_t*);

/// Consume the first n bytes of data in pn_connection_engine_read_buffer() and
/// update the buffer.
PN_EXTERN void pn_connection_engine_read_done(pn_connection_engine_t*, size_t n);

/// Close the read side of the transport when no more data is available.
/// Note there may still be events for pn_connection_engine_dispatch() or data
/// in pn_connection_engine_write_buffer()
PN_EXTERN void pn_connection_engine_read_close(pn_connection_engine_t*);

/// The engine's write buffer. Write data from buf.start to your IO destination,
/// up to a max of buf.size. Then call pn_connection_engine_write_done().
///
/// buf.size==0 means the engine has nothing to write presently.  Calling
/// pn_connection_engine_dispatch() may generate more data.
PN_EXTERN pn_bytes_t pn_connection_engine_write_buffer(pn_connection_engine_t*);

/// Call when the first n bytes of pn_connection_engine_write_buffer() have been
/// written to IO and can be re-used for new data.  Updates the buffer.
PN_EXTERN void pn_connection_engine_write_done(pn_connection_engine_t*, size_t n);

/// Call when the write side of IO has closed and no more data can be written.
/// Note that there may still be events for pn_connection_engine_dispatch() or
/// data to read into pn_connection_engine_read_buffer().
PN_EXTERN void pn_connection_engine_write_close(pn_connection_engine_t*);

/// Close both sides of the transport, equivalent to
///     pn_connection_engine_read_close(); pn_connection_engine_write_close()
///
/// You must still call pn_connection_engine_dispatch() to process final
/// events.
///
/// To provide transport error information to the handler, set it on
///     pn_connection_engine_condition()
/// *before* calling pn_connection_engine_disconnected(). This sets
/// the error on the pn_transport_t object.
///
/// Note this does *not* modify the pn_connection_t, so you can distinguish
/// between a connection close error sent by the remote peer (which will set the
/// connection condition) and a transport error (which sets the transport
/// condition.)
///
PN_EXTERN void pn_connection_engine_disconnected(pn_connection_engine_t*);

/// Get the next available event.
/// Call in a loop until it returns NULL to dispatch all available events.
/// Note this call may modify the read and write buffers.
///
/// @return Pointer to the next event, or NULL if there are none available.
///
PN_EXTERN pn_event_t* pn_connection_engine_dispatch(pn_connection_engine_t*);

/// Return true if the engine is finished - all data has been written, all
/// events have been handled and the transport is closed.
PN_EXTERN bool pn_connection_engine_finished(pn_connection_engine_t*);

/// Get the AMQP connection, owned by the pn_connection_engine_t.
PN_EXTERN pn_connection_t* pn_connection_engine_connection(pn_connection_engine_t*);

/// Get the proton transport, owned by the pn_connection_engine_t.
PN_EXTERN pn_transport_t* pn_connection_engine_transport(pn_connection_engine_t*);

/// Get the condition object for the engine's transport.
///
/// Note that IO errors should be set on this, the transport condition, not on
/// the pn_connection_t condition. The connection's condition is for errors
/// received via the AMQP protocol, the transport condition is for errors in the
/// the IO layer such as a socket read or disconnect errors.
///
PN_EXTERN pn_condition_t* pn_connection_engine_condition(pn_connection_engine_t*);

///@}

#ifdef __cplusplus
}
#endif

#endif // PROTON_CONNECTION_ENGINE_H
