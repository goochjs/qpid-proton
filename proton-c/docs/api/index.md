Proton Documentation            {#index}
====================

The Proton @ref engine API is a protocol engine that allows you to receive and
send messages on a single AMQP @ref connection.

The @ref container API manages a set of connections and lets you connect and listen

For a multi-threaded application, each AMQP @ref connection instance is not
thread safe, but separate instances are independent and can be handled
concurrently. The @ref container manages multiple connections and provides a
portable, thread-safe way to "inject" code into a connection's event loop.

The @ref io_integration is a "Service Provided Interface" to run the proton @ref
engine on a different IO frameworks or networking library. An application
written to the [Engine API](@ref engine) will work on any IO integration.

There are two historical APIs for backward compatibility, not recommended for new projects:
- The [Reactor](@ref reactor) is a TCP-only, single-threaded [Engine](@ref engine)-based API, similar to the [Container](@ref container)
- The [Messenger](@ref messenger) API hides some details of using the [Engine](@ref engine) but is less flexible.
