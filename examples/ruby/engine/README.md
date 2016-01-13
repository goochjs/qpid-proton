
The `ConnectionEngine` is an alternative to the `Reactor`. It is similar in use
but there are important differences, see the end of this README for details.

## Examples

### Broker

A sample broker which can be used by other examples and which also provides an
example itself. For now we'll just start the broker and tell it to listen on
port 8888:

    $ ruby broker.rb  --address=0.0.0.0:8888
    Listening on 0.0.0.0:8888

The broker will receive messages, create queues as needed, and deliver messages
to subscribers. More details of how it works below.

### Hello World Using A Broker

Our first example creates links to and from a queue on the broker, sends a
message, and then receives it.

To start it, simply run:

    $ ruby helloworld.rb --address=0.0.0.0:8888 --queue=examples
    Hello world!

The following events occur when `helloworld.rb` runs:

 * `on_start` - Fired when the application is started.
 * `on_sendable` - Fired when a message can be sent.
 * `on_message` - Fired when a message is received.

## More Complex Examples

Now that we've covered the basics with the archetypical hello world app, let's
look at some more interesting examples.

There are four example applications that demonstrate how to send and receive
messages both directly and through an intermediary, such as a broker:

 * `simple_send.rb` - sends messages to a receiver at a specific address and
   receives responses via an intermediary,
 * `simple_recv.rb` - receives messages from via an intermediary,
 * `direct_send.rb` - sends messages directly to a receiver and listens for
   responses itself, and
 * `direct_recv.rb` - receives messages directly.

The `simple` versions are *clients*. `simple_send.rb` connects to an address sends
a message. `simple_recv.rb` connects and receives a message.

The `direct` versions are *servers* . They accept a connection and wait for a client
to connect, then send or receive a message.

These programs plus the broker illustrate the main actors in a messaging system:
clients, servers and intermediaries.

 You can use the examples in the follow ways:

    simple_send.rb -> broker.rb <- simple_recv.rb
    simple_send.rb -> direct_recv.rb
    direct_send.rb -> simple_recv.rb

## How the broker works

The `broker.rb` example application is a nice demonstration of doing something
more interesting in Ruby with Proton.

The broker listens for incoming connections and links and associates the links
with queues based on the address in the link.

The classes in the broker example are:

 * `Broker`
   - accepts connections with `TCPServer`, creates a `ConnectionEngine` for each
   - extends `MessagingHandler` with event handler methods that create queues
     and transfer messages
 * `MessageQueue`
   - holds queued messages and tracks the links that are subscribed to the
     queue.

The broker demonstrates a new set of events:

 * `on_link_opening` - Fired when a remote link is opened but the local end is
   not yet open. From this event the broker grabs the address and subscribes the
   link to an exchange for that address.
 * `on_link_closing` - Fired when a remote link is closed but the local end is
   still open. From this event the broker grabs the address and unsubscribes the
   link from that exchange.
 * `on_connection_closing` - Fired when a remote connection is closed but the
   local end is still open.
 * `on_disconnected` - Fired when the protocol transport has closed. The broker
   removes all links for the disconnected connection, avoiding workign with
   endpoints that are now gone.

## What is the ConnectionEngine

The ConnectionEngine is an alternative to the Reactor.

Application code extends `Qpid::Proton::Handler::MessagingHandler` to handle
AMQP event methods, and uses the core Proton classes like `Connection`, `Link`
and so on to initiate AMQP actions. All this is the same as when using the Reactor.

However, the `Reactor` is a single-threaded framework that handles listening,
connecting, polling, IO and AMQP processing for multiple connections. It is
implemented in C, which means it blocks any other ruby thread in the process.

By contrast, a `ConnectionEngine` *only* does AMQP processing, and *only* for
one connection. Multiple engines can be used to get the same features as the
reactor but with greater user control over IO and threads.

### You control how the IO works.

The engine only requires an object that responds to `read_nonblock`,
`write_nonblock` like a standard `IO` object.  Any object that provides access
to a two-way byte stream can be used as the basis of an AMQP connection.

On the client side, there are some convenient methods to connect using
`TCPSocket` since that is a commmon case, but you can construct an engine
directly using any IO object you wish.

There is a broker example showing how to implement a full server using
`TCPSocket` and `select` but again you can use any form of IO, or use a
framework other than select, such as some popular event-based IO frameworks.

### You control how/if you use threads.

The engine itself is not thread safe, but you can use separate engines in
separate threads. For example:

Single threaded: you can manage multiple connections in a single thread using
something like `select`, in a server or a client.

Blocking: No extra threads, the application runs AMQP processing in
its own thread as needed.

Thread-per-connection: Start a separate thread for each connection and handle
events in that thread.

Thread pool: Use some form of thread-safe select or poll to delegate to a fixed
thread pool. Connections can be processed by any thread in the pool, but each
connection is processed by only one thread at a time.

