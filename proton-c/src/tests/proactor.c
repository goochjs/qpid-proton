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

#include "test_tools.h"
#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/event.h>
#include <proton/listener.h>
#include <proton/proactor.h>
#include <proton/transport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pn_millis_t timeout = 5*1000; /* timeout for hanging tests */

static const char *localhost = "127.0.0.1"; /* host for connect/listen */

struct test_events {
  pn_proactor_t *proactor;
  pn_event_batch_t *events;
};

/* Wait for the next single event, return its type */
static pn_event_type_t wait_next(pn_proactor_t *proactor) {
  pn_event_batch_t *events = pn_proactor_wait(proactor);
  pn_event_type_t etype = pn_event_type(pn_event_batch_next(events));
  pn_proactor_done(proactor, events);
  return etype;
}

/* Get events until an event of `type` or a PN_TRANSPORT_CLOSED/PN_PROACTOR_TIMEOUT */
static pn_event_type_t wait_for(pn_proactor_t *proactor, pn_event_type_t etype) {
  while (true) {
    pn_event_type_t t = wait_next(proactor);
    if (t == etype || t == PN_PROACTOR_TIMEOUT) {
      return t;
    }
  }
}

/* Test that interrupt and timeout events cause pn_proactor_wait() to return. */
static void test_interrupt_timeout(test_t *t) {
  pn_proactor_t *p = pn_proactor();
  pn_proactor_interrupt(p);
  pn_event_type_t etype = wait_next(p);
  TEST_CHECK(t, PN_PROACTOR_INTERRUPT == etype, pn_event_type_name(etype));
  pn_proactor_set_timeout(p, 1); /* very short timeout */
  etype = wait_next(p);
  TEST_CHECK(t, PN_PROACTOR_TIMEOUT == etype, pn_event_type_name(etype));
  pn_proactor_free(p);
}

/* Test handler return value  */
typedef enum {
  H_CONTINUE,                   /**@<< handler wants more events */
  H_FINISHED,                   /**@<< handler completed without error */
  H_FAILED                      /**@<< handler hit an error and cannot continue */
} handler_state_t;

typedef handler_state_t (*test_handler_fn)(test_t *, pn_event_t*);

/* Proactor and handler that take part in a test */
typedef struct proactor_test_t {
  test_t *t;
  test_handler_fn handler;
  pn_proactor_t *proactor;
  handler_state_t state;                    /* Result of last handler call */
} proactor_test_t;


/* Initialize an array of proactor_test_t */
static void proactor_test_init(proactor_test_t *pts, size_t n) {
  for (proactor_test_t *pt = pts; pt < pts + n; ++pt) {
    if (!pt->proactor) pt->proactor = pn_proactor();
    pn_proactor_set_timeout(pt->proactor, timeout);
    pt->state = H_CONTINUE;
  }
}

/* Iterate over an array of proactors, draining or handling events with the non-blocking
   pn_proactor_get.  Continue till all handlers return H_FINISHED (and return 0) or one
   returns H_FAILED  (and return non-0)
*/
static int proactor_test_run(proactor_test_t *pts, size_t n) {
  /* Make sure pts are initialized */
  proactor_test_init(pts, n);
  size_t finished = 0;
  do {
    finished = 0;
    for (proactor_test_t *pt = pts; pt < pts + n; ++pt) {
      pn_event_batch_t *events = pn_proactor_get(pt->proactor);
      if (events) {
          pn_event_t *e;
          while ((e = pn_event_batch_next(events))) {
            if (pt->state == H_CONTINUE) {
              pt->state = pt->handler(pt->t, e);
            }
          }
          pn_proactor_done(pt->proactor, events);
      }
      switch (pt->state) {
       case H_CONTINUE: break;
       case H_FINISHED: ++finished; break;
       case H_FAILED: return 1;
      }
    }
  } while (finished < n);
  return 0;
}


/* Handler for test_listen_connect, does both sides of the connection */
static handler_state_t listen_connect_handler(test_t *t, pn_event_t *e) {
  pn_connection_t *c = pn_event_connection(e);
  pn_listener_t *l = pn_event_listener(e);

  switch (pn_event_type(e)) {
    /* Act on these events */
   case PN_LISTENER_ACCEPT: {
    pn_connection_t *accepted = pn_connection();
    pn_connection_open(accepted);
    pn_listener_accept(l, accepted); /* Listener takes ownership of accepted */
    return H_CONTINUE;
   }

   case PN_CONNECTION_REMOTE_OPEN:
    if (pn_connection_state(c) | PN_LOCAL_ACTIVE) { /* Client is fully open - the test is done */
      pn_connection_close(c);
    }  else {                   /* Server returns the open */
      pn_connection_open(c);
    }
    return H_CONTINUE;

   case PN_CONNECTION_REMOTE_CLOSE:
    if (pn_connection_state(c) | PN_LOCAL_ACTIVE) {
      pn_connection_close(c);    /* Return the close */
    }
    return H_CONTINUE;

   case PN_TRANSPORT_CLOSED:
    return H_FINISHED;

   default:
    return H_CONTINUE;
    break;
  }
}

/* Test bad-address error handling for listen and connect */
static void test_early_error(test_t *t) {
  pn_proactor_t *p = pn_proactor();
  pn_proactor_set_timeout(p, timeout); /* In case of hang */
  pn_connection_t *c = pn_connection();
  pn_proactor_connect(p, c, localhost, "1"); /* Bad port */
  pn_event_type_t etype = wait_for(p, PN_TRANSPORT_CLOSED);
  TEST_CHECK(t, PN_TRANSPORT_CLOSED == etype, pn_event_type_name(etype));
  TEST_CHECK(t, pn_condition_is_set(pn_transport_condition(pn_connection_transport(c))), "");

  pn_listener_t *l = pn_listener();
  pn_proactor_listen(p, l, localhost, "1", 1); /* Bad port */
  etype = wait_for(p, PN_LISTENER_CLOSE);
  TEST_CHECK(t, PN_LISTENER_CLOSE == etype, pn_event_type_name(etype));
  TEST_CHECK(t, pn_condition_is_set(pn_listener_condition(l)), "");

  pn_proactor_free(p);
}

/* Simplest client/server interaction with 2 proactors */
static void test_listen_connect(test_t *t) {
  proactor_test_t pts[] =  { { t, listen_connect_handler }, { t, listen_connect_handler } };
  proactor_test_init(pts, 2);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port();          /* Hold a port */

  pn_proactor_listen(server, pn_listener(), localhost, port.str, 4);
  pn_event_type_t etype = wait_for(server, PN_LISTENER_OPEN);
  if (TEST_CHECK(t, PN_LISTENER_OPEN == etype, pn_event_type_name(etype))) {
    sock_close(port.sock);
    pn_proactor_connect(client, pn_connection(), localhost, port.str);
    proactor_test_run(pts, 2);
  }
  pn_proactor_free(client);
  pn_proactor_free(server);
}

static handler_state_t connection_wakeup_handler(test_t *t, pn_event_t *e) {
  pn_connection_t *c = pn_event_connection(e);
  switch (pn_event_type(e)) {

   case PN_CONNECTION_REMOTE_OPEN:
    if (pn_connection_state(c) | PN_LOCAL_UNINIT) {
      pn_connection_open(c);    /* Server returns the open */
    }
    return H_FINISHED;          /* Finish when open at both ends */

   default:
    /* Otherwise same as listen_connect_handler */
    return listen_connect_handler(t, e);
  }
}

/* Test waking up a connection that is idle */
static void test_connection_wakeup(test_t *t) {
  proactor_test_t pts[] =  { { t, connection_wakeup_handler }, { t, connection_wakeup_handler } };
  proactor_test_init(pts, 2);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port();          /* Hold a port */
  pn_proactor_listen(server, pn_listener(), localhost, port.str, 4);
  pn_event_type_t etype = wait_for(server, PN_LISTENER_OPEN);
  if (TEST_CHECK(t, PN_LISTENER_OPEN == etype, pn_event_type_name(etype))) {
    sock_close(port.sock);
    pn_connection_t *c = pn_connection();
    pn_proactor_connect(client, c, localhost, port.str);
    proactor_test_run(pts, 2);                          /* Will finish when client is connected */
    TEST_CHECK(t, NULL == pn_proactor_get(client), ""); /* Should be idle */
    pn_connection_wake(c);
    etype = wait_next(client);
    /* FIXME aconway 2017-02-21: TEST_EVENT_TYPE */
    TEST_CHECK(t, PN_CONNECTION_WAKE == etype, pn_event_type_name(etype));
  }
  pn_proactor_free(client);
  pn_proactor_free(server);
}

/* Test that INACTIVE event is generated when last connections/listeners closes. */
static void test_inactive(test_t *t) {
  proactor_test_t pts[] =  { { t, listen_connect_handler }, { t, listen_connect_handler }};
  proactor_test_init(pts, 2);
  pn_proactor_t *client = pts[0].proactor, *server = pts[1].proactor;
  test_port_t port = test_port();          /* Hold a port */

  pn_listener_t *l = pn_listener();
  pn_proactor_listen(server, l, localhost, port.str,  4);
  pn_event_type_t etype = wait_for(server, PN_LISTENER_OPEN);
  if (TEST_CHECK(t, PN_LISTENER_OPEN == etype, pn_event_type_name(etype))) {
    sock_close(port.sock);
    pn_proactor_connect(client, pn_connection(), localhost, port.str);
    proactor_test_run(pts, 2);
    etype = wait_for(client, PN_PROACTOR_INACTIVE);
    pn_listener_close(l);
    etype = wait_for(server, PN_PROACTOR_INACTIVE);
  }
  pn_proactor_free(client);
  pn_proactor_free(server);
}

int main(int argc, char **argv) {
  int failed = 0;
  RUN_ARGV_TEST(failed, t, test_inactive(&t));
  RUN_ARGV_TEST(failed, t, test_interrupt_timeout(&t));
  RUN_ARGV_TEST(failed, t, test_early_error(&t));
  RUN_ARGV_TEST(failed, t, test_listen_connect(&t));
  RUN_ARGV_TEST(failed, t, test_connection_wakeup(&t));
  return failed;
}
