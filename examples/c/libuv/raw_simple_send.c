/*
 *
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
 *
 */

#include <proton/io/connection_engine.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/message.h>
#include <proton/link.h>
#include <proton/session.h>
#include <proton/transport.h>
#include <proton/url.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include "pncompat/misc_funcs.inc"


/* FIXME aconway 2016-07-04: Better separation of proton and libuv code. */


// Extra  book-keeping for a proton_connection_engine used with libuv.
//
// In this integration, we keep at most one async read and one async write
// pending whenever we can. libuv reads/writes directly to/from the
// pn_connection_engine buffers so there is no extra copying. All proton
// calls are processed in a single libuv event loop thread.
//
typedef struct pn_uv_engine_t {
    pn_connection_engine_t engine;
    bool reading;               /* True if an async read is pending. */
    bool writing;               /* True if an async write is pending. */
    uv_write_t write;           /* The async write callback object. */
} pn_uv_engine_t;

// Application data
typedef struct app_data_t {
    pn_uv_engine_t engine;

    const char* address;
    pn_rwbytes_t message_buffer;
    int message_count;
    int sent;
    int acknowledged;
} app_data_t;

/* FIXME aconway 2016-07-04: Order for pn_bytes(size, start) */

// Create a message with a map { "sequence" : number } encode it and return the encoded buffer.
static pn_bytes_t encode_message(app_data_t* app) {
    /* Construct a message with the map { "sequence": app.sent } */
    pn_message_t* message = pn_message();
    pn_data_put_int(pn_message_id(message), app->sent); /* Set the message_id also */
    pn_data_t* body = pn_message_body(message);
    pn_data_put_map(body);
    pn_data_enter(body);
    pn_data_put_string(body, pn_bytes(sizeof("sequence")-1, "sequence"));
    pn_data_put_int(body, app->sent); /* The sequence number */
    pn_data_exit(body);

    // encode the message, expanding the encode buffer as needed
    //
    if (app->message_buffer.start == NULL) {
        static const size_t initial_size = 128;
        app->message_buffer = pn_rwbytes(initial_size, (char*)malloc(initial_size));
    }
    /* app->message_buffer is the total buffer space available. */
    /* mbuf wil point at just the portion used by the encoded message */
    pn_rwbytes_t mbuf = pn_rwbytes(app->message_buffer.size, app->message_buffer.start);
    int status = 0;
    while ((status = pn_message_encode(message, mbuf.start, &mbuf.size)) == PN_OVERFLOW) {
        app->message_buffer.size *= 2;
        app->message_buffer.start = (char*)realloc(app->message_buffer.start, app->message_buffer.size);
        mbuf.size = app->message_buffer.size;
    }
    if (status != 0) {
        fprintf(stderr, "error encoding message: %s\n", pn_error_text(pn_message_error(message)));
        exit(1);
    }
    pn_message_free(message);
    return pn_bytes(mbuf.size, mbuf.start);
}

/* FIXME aconway 2016-07-04: uv */
app_data_t* app_data(uv_handle_t* handle) { return (app_data_t*)handle->data; }

/* FIXME aconway 2016-07-04: naming - use of handle() vs. UV. */
// Process a proton event for the application app.
static void process(app_data_t* app, pn_event_t* event) {

    switch (pn_event_type(event)) {

      case PN_CONNECTION_INIT: {
          /* FIXME aconway 2016-07-04: move to main or keep here? */
          pn_connection_t* c = pn_event_connection(event);
          pn_connection_set_container(c, "simple_send.c");
          pn_connection_open(c);
          pn_session_t* s = pn_session(c);
          pn_session_open(s);
          pn_link_t* l = pn_sender(s, "my_sender");
          pn_terminus_set_address(pn_link_target(l), app->address);
          pn_link_open(l);
      } break;

      case PN_LINK_FLOW: {
          // The peer has given us some credit, now we can send messages
          pn_link_t *sender = pn_event_link(event);
          while (pn_link_credit(sender) > 0 && app->sent < app->message_count) {
              ++app->sent;
              /* FIXME aconway 2016-06-30: explain delivery tag/delivery */
              pn_delivery(sender, pn_dtag((const char *)&app->sent, sizeof(app->sent)));
              pn_bytes_t msgbuf = encode_message(app);
              pn_link_send(sender, msgbuf.start, msgbuf.size);
              pn_link_advance(sender);
          }
      } break;

      case PN_DELIVERY: {
          // We received acknowledgedment from the peer that a message was delivered.
          pn_delivery_t* d = pn_event_delivery(event);
          if (pn_delivery_remote_state(d) == PN_ACCEPTED) {
              if (++app->acknowledged == app->message_count) {
                  printf("%d messages sent and acknowledged\n", app->acknowledged);
                  pn_connection_close(pn_event_connection(event));
              }
          }
      } break;

      case PN_TRANSPORT_CLOSED: {
          pn_transport_t *tport = pn_event_transport(event);
          pn_condition_t *cond = pn_transport_condition(tport);
          if (pn_condition_is_set(cond)) {
              fprintf(stderr, "transport error: %s: %s\n",
                      pn_condition_get_name(cond), pn_condition_get_description(cond));
          }
      } break;

      default: break;
    }
}

static void on_close(uv_handle_t* handle) {
    pn_connection_engine_t *engine = &app_data(handle)->engine.engine;
    pn_connection_engine_disconnected(engine);
    /* Handle final events */
    pn_event_t* event;
    while ((event = pn_connection_engine_dispatch(engine)) != NULL) {
        process(app_data(handle), event);
    }
    pn_connection_engine_final(engine);
    free(app_data(handle)->message_buffer.start);
}

static void on_write(uv_write_t* request, int status);
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

// Read buffer allocation function just returns the engine's read buffer.
static void alloc_read_buffer(uv_handle_t* stream, size_t size, uv_buf_t* buf) {
    pn_connection_engine_t *engine = &app_data(stream)->engine.engine;
    pn_rwbytes_t rbuf = pn_connection_engine_read_buffer(engine);
    *buf = uv_buf_init(rbuf.start, rbuf.size);
}

static void do_work(uv_stream_t* stream) {
    /* FIXME aconway 2016-07-04: ugly */
    pn_uv_engine_t *uvengine = &app_data((uv_handle_t*)stream)->engine;
    pn_connection_engine_t *engine = &uvengine->engine;
    pn_event_t* event;
    while ((event = pn_connection_engine_dispatch(engine)) != NULL) {
        process(app_data((uv_handle_t*)stream), event);          /* FIXME aconway 2016-06-29:  */
    }
    pn_bytes_t wbuf = pn_connection_engine_write_buffer(engine);
    if (wbuf.size > 0 && !uvengine->writing) {
        uvengine->writing = true;
        uv_buf_t buf = uv_buf_init((char*)wbuf.start, wbuf.size); /* FIXME aconway 2016-06-30: const */
        uv_write(&uvengine->write, stream, &buf, 1, on_write);
    }
    pn_rwbytes_t rbuf = pn_connection_engine_read_buffer(engine);
    if (rbuf.size > 0 && !uvengine->reading) {
        uvengine->reading = true;
        uv_read_start(stream, alloc_read_buffer, on_read);
    }
    if (pn_connection_engine_finished(engine)) {
        uv_close((uv_handle_t*)stream, on_close);
    }
}

/* Copy the UV error state to the pn_connection_engine. */
void set_uv_error(pn_connection_engine_t* engine, int err, const char* what) {
    if (err < 0 && err != UV_EOF) {
        pn_condition_t* condition = pn_connection_engine_condition(engine);
        if (!pn_condition_is_set(condition)) {
            pn_condition_set_name(condition, uv_err_name(err));
            pn_condition_set_description(condition, uv_strerror(err));
        }
    }
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    pn_uv_engine_t *uvengine = &app_data((uv_handle_t*)stream)->engine;
    pn_connection_engine_t *engine = &uvengine->engine;
    uvengine->reading = false;
    if(nread >= 0) {
        pn_connection_engine_read_done(engine, nread);
        uv_read_stop(stream);
    } else if (nread == UV_EOF) { /* hangup */
        pn_connection_engine_read_close(engine);
    } else {                    /* error */
        set_uv_error(engine, nread, "read");
        pn_connection_engine_disconnected(engine);
    }
    do_work(stream);
}

void on_write(uv_write_t* request, int status) {
    pn_uv_engine_t *uvengine = &app_data((uv_handle_t*)request->handle)->engine;
    pn_connection_engine_t *engine = &uvengine->engine;
    uvengine->writing = false;
    if (status == 0) {
        pn_bytes_t wbuf = pn_connection_engine_write_buffer(engine);
        pn_connection_engine_write_done(engine, wbuf.size);
    } else {
        set_uv_error(engine, status, "write");
        pn_connection_engine_disconnected(engine);
    }
    do_work(request->handle);
}

static void on_connect(uv_connect_t* connect, int status) {
    if (status < 0) {
        fprintf(stderr, "cannot connect: %s\n", uv_strerror(status));
        return;
    }
    pn_connection_engine_t *engine = &app_data((uv_handle_t*)connect->handle)->engine.engine;
    if (pn_connection_engine_init(engine)) {
        fprintf(stderr, "engine init failed");
        uv_close((uv_handle_t*)connect->handle, on_close);
        return;
    }
    do_work(connect->handle);
}

static void usage(void) {
    printf("Usage: simple_send [-a url] [-m message-count]\n");
}

static const char* str_or_default(const char* str, const char* def) {
    return (str && *str) ? str : def;
}

static void check(int err, const char* msg) {
    if (err) {
        fprintf(stderr, "%s: %s\n", msg, uv_strerror(err));
        exit(1);
    }
}

int main(int argc, char **argv) {
    /* Default values for application and connection. */
    app_data_t app = {0};
    app.address = "example";
    app.message_count = 100;
    const char* host = 0;
    const char* port = "amqp";
    pn_url_t* url = 0;

    opterr = 0;
    int c;
    while((c = getopt(argc, argv, "a:m:")) != -1) {
        switch(c) {
          case 'a': {           /* parse the URL */
              url = pn_url_parse(optarg);
              app.address = str_or_default(pn_url_get_path(url), app.address);
              host = str_or_default(pn_url_get_host(url), host);
              port = str_or_default(pn_url_get_port(url), port);
          } break;

          case 'm':
            app.message_count = atoi(optarg); break;

          default:
            usage();
            return 1;
        }
    }
    if (optind < argc) {
        usage();
        return 1;
    }

    int err;

    uv_loop_t* loop = uv_default_loop();

    uv_getaddrinfo_t addrinfo = {0};
    if ((err = uv_getaddrinfo(loop, &addrinfo, NULL, host, port, NULL))) {
        fprintf(stderr, "cannot resolve %s:%s: %s\n", host ? host:"", port, uv_strerror(err));
        return 1;
    }

    uv_tcp_t socket = {0};
    check(uv_tcp_init(loop, &socket), "tcp_init");
    socket.data = &app;         /* Set application data on UV connection. */

    uv_connect_t connect_req;
    check(uv_tcp_connect(&connect_req, &socket, addrinfo.addrinfo->ai_addr, on_connect), "connect");
    check(uv_run(loop, UV_RUN_DEFAULT), "uv_run");

    uv_freeaddrinfo(addrinfo.addrinfo);
    if (url) pn_url_free(url);
    return 0;
}
