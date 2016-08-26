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

#include <proton/container.h>

#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/session.h>
#include <proton/transport.h>
#include <proton/url.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pncompat/misc_funcs.inc" /* FIXME aconway 2016-08-16: ?? */

// Application data
typedef struct app_data_t {
    int message_count;
    int sent;
    int acknowledged;
} app_data_t;

/* FIXME aconway 2016-07-19: move to core message API */

/**
 * Encode a pn_message_t as AMQP formatted binary data into *buffer.
 * If the *buffer is NULL or *size is not sufficient, call alloc_fn to
 * allocate sufficient space and free_fn to free the old buffer. Will
 * not allocate more space than is required to encode the message.
 *
 * Updates *buffer and *size to the final buffer location and encoded
 * size of the message. The caller is responsible for freeing the
 * final value of *buffer.
 *
 * @param[in] msg a message object
 * @param[in] buffer the start of the buffer for encoded AMQP data.
 *   If NULL, a buffer will be allocated by calling alloc_fn.
 * @param[in] size the size the initial buffer.
 * @param[out] size the size of the encoded message.
 * @param[in] alloc_fn a function to allocate memory, such as malloc(3)
 * @param[in] free_fn a function to free memory, such as free(3)
 * @return zero on success or an error code on failure
 */
static int pn_message_encode_alloc(
    pn_message_t *msg, char **buffer, size_t *size,
    void* (*alloc_fn)(size_t), void (*free_fn)(void*))
{
    /* FIXME aconway 2016-07-19: this implementation is not correct, it over-allocates.
       Placeholder for the real implementation. */
    if (*buffer == NULL || *size == 0) {
        free_fn(*buffer);
        if (*size == 0) {
            *size = 128;
        }
        *buffer = alloc_fn(*size);
    }
    int status = 0;
    while ((status = pn_message_encode(msg, *buffer, size)) == PN_OVERFLOW) {
        free_fn(*buffer);
        *size *= 2;
        *buffer = alloc_fn(*size);
    }
    return status;
}

/* FIXME aconway 2016-07-19: error checking */

/* Send the next message */
static void send_message(app_data_t *app, pn_link_t *sender) {
    pn_message_t *message = pn_message();
    int sequence = app->sent;   /* Sequence number for the message */
    /* Set the message_id to the sequence number  */
    pn_data_put_int(pn_message_id(message), sequence);
    /* Set the body to a map { "sequence" : sequence } */
    pn_data_t* body = pn_message_body(message);
    pn_data_put_map(body);
    pn_data_enter(body);
    pn_data_put_string(body, pn_bytes(sizeof("sequence")-1, "sequence"));
    pn_data_put_int(body, sequence);
    pn_data_exit(body);
    char* encoded = 0;
    size_t size = 0;
    pn_message_encode_alloc(message, &encoded, &size, &malloc, &free);
    pn_message_free(message);
    pn_link_send(sender, encoded, size);
    pn_link_advance(sender);
    free(encoded);
}

/* Handler function to handle proton events for the application. */
static void handler(pn_event_t* event) {
    app_data_t *app = (app_data_t*)pn_connection_get_context(pn_event_connection(event));
    pn_container_t* container = pn_event_container(event);

    switch (pn_event_type(event)) {

      case PN_LINK_FLOW: {
          // The peer has given us some credit, now we can send messages
          pn_link_t *sender = pn_event_link(event);
          while (pn_link_credit(sender) > 0 && app->sent < app->message_count) {
              ++app->sent;
              /* FIXME aconway 2016-06-30: explain delivery tag/delivery */
              pn_delivery(sender, pn_dtag((const char *)&app->sent, sizeof(app->sent)));
              send_message(app, sender);
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
          pn_container_stop(container);
      } break;

      default: break;
    }
}

static void usage(void) {
    printf("Usage: simple_send [-a url] [-m message-count]\n");
}

int main(int argc, char **argv) {
    const char *url = "localhost:amqp/example";
    int messages = 100;

    opterr = 0;
    int c;
    while((c = getopt(argc, argv, "a:m:")) != -1) {
        switch(c) {
          case 'a':
            url = optarg;
            break;
          case 'm':
            messages = atoi(optarg);
            break;
          default:
            usage();
            return 1;
        }
    }
    if (optind < argc) {
        usage();
        return 1;
    }

    /* Context data for the handler */
    app_data_t app = { 0 };
    app.message_count = messages;

    pn_container_t *container = pn_container("simple_send");

    /* FIXME aconway 2016-08-16: rewrite following C++ example */
    pn_connection_t* connection = pn_container_connect(container, url);
    pn_session_t* session = pn_session(connection);
    pn_session_open(session);
    pn_link_t* sender = pn_sender(session, "my_sender");
    pn_url_t* purl = pn_url_parse(url);
    if (purl == NULL) {
        fprintf(stderr, "Invalid URL: %s\n", url);
        return 1;
    }
    pn_terminus_set_address(pn_link_target(sender), pn_url_get_path(purl));
    pn_url_free(purl);
    pn_link_open(sender);

    pn_container_run(container);
    pn_container_free(container);
    /* The connection, session and link objects are cleaned up with the container. */
    return 0;
}

/* FIXME aconway 2016-07-19: remove */
