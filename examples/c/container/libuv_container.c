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
#include <proton/io/container_impl.h>

#include <proton/connection.h>
#include <proton/container.h>
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

/* FIXME aconway 2016-07-19: doc a libuv-based C container */

typedef struct pn_uv_container_t {
    pn_container_t container;
    uv_loop_t* loop;
} pn_uv_container_t;

// Get the uv_loop_t from a pn_container_t* that points to a pn_uv_container_t
static uv_loop_t* pn_uv_loop(pn_container_t* c) {
    return ((pn_uv_container_t*)c)->loop;
}

/* FIXME aconway 2016-08-26: all C style comments. */
// Implementation for pn_container_free
static void pn_uv_free(pn_container_t *pnc) {
    pn_uv_container_t* uvc = (pn_uv_container_t*)pnc;
    if (uvc) {
        free((char*)uvc->container.id);
        free(uvc);
    }
    /* FIXME aconway 2016-08-16: clean up loop? using uv_default_loop. */
}

#error FIXME HERE

// Implementation for pn_container_connect
static int pn_uv_connect(pn_container_t *c, const char* url_str) {
    pn_url_t *url = pn_url_parse(url_str);
    const char* host = pn_url_get_host(url);
    const char* port = pn_url_get_port(url);
    port = port ? port : "5672"; /* Default port */
    uv_getaddrinfo_t addrinfo = {0};
    if ((err = uv_getaddrinfo(pn_uv_loop(c), &addrinfo, NULL, host, port, NULL))) {
        fprintf(stderr, "cannot resolve %s:%s: %s\n", host ? host:"", port, uv_strerror(err));
        return 1;
    }

    uv_tcp_t socket = {0};
    check(uv_tcp_init(loop, &socket), "tcp_init");
    socket.data = &app;         /* Set application data on UV connection. */

    uv_connect_t connect_req;
    check(uv_tcp_connect(&connect_req, &socket, addrinfo.addrinfo->ai_addr, on_connect), "connect");
}

// Extra book-keeping for connection_engine used with libuv.
typedef struct pn_uv_engine_t {
    pn_connection_engine_t engine;
    bool reading;               /* True if an async read is pending. */
    bool writing;               /* True if an async write is pending. */
    uv_write_t write;           /* The async write callback object. */

    /* FIXME aconway 2016-07-19: move to generic connection_engine */
    pn_handler_fn handler;      /* Handler function. */
    void* context;              /* Context to pass to handler. */
} pn_uv_engine_t;

// Get the pn_uv_engine_t* from an arbitrary uv_handle type, passed as void*
// since all UV handle types can be cast to uv_handle_t*.
static pn_uv_engine_t* pn_uv_engine(void* uv_handle) {
    return (pn_uv_engine_t*)((uv_handle_t*)uv_handle)->data;
}

static void on_write(uv_write_t* request, int status);
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void on_close(uv_handle_t* handle);

// Read buffer allocation function just returns the engine's read buffer.
static void alloc_read_buffer(uv_handle_t* stream, size_t size, uv_buf_t* buf) {
    pn_buf_t rbuf = pn_connection_engine_read_buffer(&get_uv_engine(stream)->engine);
    *buf = uv_buf_init(rbuf.data, rbuf.size);
}

static void do_work(uv_stream_t* stream) {
    /* FIXME aconway 2016-07-04: ugly */
    pn_uv_engine_t *uv_engine = get_uv_engine(stream);
    pn_connection_engine_t *engine = &uv_engine->engine;
    pn_event_t* event;
    while ((event = pn_connection_engine_dispatch(engine)) != NULL) {
        uv_engine->handler(event, uv_engine->context);
    }
    pn_cbuf_t wbuf = pn_connection_engine_write_buffer(engine);
    if (wbuf.size > 0 && !uv_engine->writing) {
        uv_engine->writing = true;
        uv_buf_t buf = uv_buf_init((char*)wbuf.data, wbuf.size); /* FIXME aconway 2016-06-30: const */
        uv_write(&uv_engine->write, stream, &buf, 1, on_write);
    }
    pn_buf_t rbuf = pn_connection_engine_read_buffer(engine);
    if (rbuf.size > 0 && !uv_engine->reading) {
        uv_engine->reading = true;
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
    pn_uv_engine_t *uv_engine = get_uv_engine(stream);
    pn_connection_engine_t *engine = &uv_engine->engine;
    uv_engine->reading = false;
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
    pn_uv_engine_t *uv_engine = get_uv_engine(request->handle);
    pn_connection_engine_t *engine = &uv_engine->engine;
    uv_engine->writing = false;
    if (status == 0) {
        pn_cbuf_t wbuf = pn_connection_engine_write_buffer(engine);
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
    pn_connection_engine_t *engine = &get_uv_engine(connect->handle)->engine;
    if (pn_connection_engine_init(engine)) {

        /* FIXME aconway 2016-07-19: error handling */
        fprintf(stderr, "engine init failed");
        uv_close((uv_handle_t*)connect->handle, on_close);
        return;
    }
    do_work(connect->handle);
}

static void on_close(uv_handle_t* handle) {
    pn_uv_engine_t *uv_engine = get_uv_engine(handle);
    pn_connection_engine_t *engine = &uv_engine->engine;
    pn_connection_engine_disconnected(engine);
    /* Handle final events */
    pn_event_t* event;
    while ((event = pn_connection_engine_dispatch(engine)) != NULL) {
        uv_engine->handler(event, uv_engine->context);
    }
    pn_connection_engine_final(engine);
}

/* FIXME aconway 2016-07-19: HERE needed for error checking. Report via uv loop? */

static void check(int err, const char* msg) {
    if (err) {
        fprintf(stderr, "%s: %s\n", msg, uv_strerror(err));
        exit(1);
    }
}

/* FIXME aconway 2016-08-16: This is the only public function */
pn_container_t *pn_uv_container(const char* id) {
    pn_uv_container_t *c = malloc(sizeof(pn_uv_container_t));
    memset(c, '\0', sizeof(pn_uv_container_t));
    /* FIXME aconway 2016-08-26: set impl functions */
    c->id = strdup(id);
    c->loop = uv_default_loop();
    return c;
}
