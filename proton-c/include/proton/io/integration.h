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

/** @file
 *
 * The proton IO Integration SPI to implement proton on new IO frameworks.
 *
 *@defgroup io_integration IO Integration SPI
 * A Service Provider Interface (SPI) that allows you to integrate proton with any
 * IO library or threading framework, in addition to using the built-in IO support.
 *
 * The SPI consists of the \ref connection_engine and \ref container_impl. These
 * are distinct from the \ref engine API used by application code, \ref engine
 * application code is decoupled from the IO integration layer.
 *
 * The \ref connection_engine manages a single AMQP connection. It converts
 * AMQP-encoded bytes from any source into \ref pn_event_t events that can be
 * handled using the \ref engine API, and generates AMQP-encoded bytes to write to
 * any destination. The \ref connection_engine is not thread safe but each instance
 * is independent so different connections can be processed concurrently.
 *
 * The \ref container_impl allows you to implement a \ref container for your
 * integration.  All the other types in the \ref engine API are providd by the
 * proton library, you implement the \ref container as an entry point to create
 * your connections.
 *
 * The \ref engine is thread and IO "agnostic": there is no IO or thread
 * synchronization code except what you provide in your \ref container_impl and
 * \ref connection_engine code.  Thus it can be integrated efficiently with single
 * or multi-threaded code, in reactive or proactive IO frameworks, and with any
 * kind of native or third party IO library.
 */

#include <proton/io/container.h>
#include <proton/io/connection_engine.h>
