#--
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#++

require 'util/id_factory'

module Qpid::Proton::Reactor

  # @private
  class InternalTransactionHandler < Qpid::Proton::Handler::OutgoingMessageHandler

    def initialize
      super
    end

    def on_settled(event)
      if event.delivery.respond_to? :transaction
        event.transaction = event.delivery.transaction
        event.delivery.transaction.handle_outcome(event)
      end
    end

  end


  # A representation of the AMQP concept of a container which, loosely
  # speaking, is something that establishes links to or from another
  # container on which messages are transferred.
  #
  # This is an extension to the Reactor classthat adds convenience methods
  # for creating instances of Qpid::Proton::Connection, Qpid::Proton::Sender
  # and Qpid::Proton::Receiver.
  #
  # @example
  #
  class Container < Reactor

    include Qpid::Proton::Util::Reactor

    attr_accessor :global_handler
    attr_reader :container_id

    def initialize(handlers, options = {})
      super(handlers, options)

      # FIXME aconway 2016-01-12: document options
      @container_id = String.new(options[:container_id] || SecureRandom.uuid).freeze
      @connection_ids = Qpid::Proton::Util::IdFactory.new @container_id

      # only do the following if we're creating a new instance
      if !options.has_key?(:impl)
        @ssl = SSLConfig.new
        if options[:global_handler]
          self.global_handler = GlobalOverrides.new(options[:global_handler])
        else
          # very ugly, but using self.global_handler doesn't work in the constructor
          ghandler = Reactor.instance_method(:global_handler).bind(self).call
          ghandler = GlobalOverrides.new(ghandler)
          Reactor.instance_method(:global_handler=).bind(self).call(ghandler)
        end
        @trigger = nil
      end
    end

    # Connect and return a Qpid::Proton::Connection. See Qpid::Proton::Connection#open
    # for options.
    def connect(options = {})
      # FIXME aconway 2016-01-14: url should be a required param?
      # FIXME aconway 2016-01-14: doc options, connection opts + handler, not container_id
      conn = self.connection(options[:handler])
      connector = Connector.new(conn)
      conn.overrides = connector
      if !options[:url].nil?
        connector.address = URLs.new([options[:url]])
      elsif !options[:urls].nil?
        connector.address = URLs.new(options[:urls])
      elsif !options[:address].nil?
        connector.address = URLs.new([Qpid::Proton::URL.new(options[:address])])
      else
        raise ::ArgumentError.new("either :url or :urls or :address required")
      end

      connector.heartbeat = options[:heartbeat] if !options[:heartbeat].nil?
      if !options[:reconnect].nil?
        connector.reconnect = options[:reconnect]
      else
        connector.reconnect = Backoff.new()
      end

      connector.ssl_domain = SessionPerConnection.new # TODO seems this should be configurable

      opts = options.merge({:container_id => @container_id,
                            :id_factory => Qpid::Proton::Util::IdFactory.new(@connection_ids.next)})
      conn.open opts
      return conn
    end

    def _session(context)
      if context.is_a?(Qpid::Proton::URL)
        return self._session(self.connect(:url => context))
      elsif context.is_a?(Qpid::Proton::Session)
        return context
      elsif context.is_a?(Qpid::Proton::Connection)
        return context.default_session
      else
        return context.session
      end
    end

    # FIXME aconway 2016-01-14: move documentation to link
    # FIXME aconway 2016-01-14: rename to open_sender
    # Initiates the establishment of a link over which messages can be sent.
    #
    # @param context [String, URL] The context.
    # @param opts [Hash] Additional options.
    # @param opts [String, Qpid::Proton::URL] The target address.
    # @param opts [String] :source The source address.
    # @param opts [Boolean] :dynamic
    # @param opts [Object] :handler
    # @param opts [Hash] :options Addtional link options
    #
    # @return [Sender] The sender.
    #
    def create_sender(context, options = {})
      # FIXME aconway 2016-01-12: naming - open_sender?
      if context.is_a?(::String)
        context = Qpid::Proton::URL.new(context)
      end
      if context.is_a?(Qpid::Proton::URL)
        options[:target] ||= context.path
      end

      session = self._session(context)
      # FIXME aconway 2016-01-14: refactor
      # self._apply_link_options(options[:options], sender)
      session.open_sender(options)
    end

    # Initiates the establishment of a link over which messages can be received.
    #
    # There are two accepted arguments for the context
    #  1. If a Connection is supplied then the link is established using that
    # object. The source, and optionally the target, address can be supplied
    #  2. If it is a String or a URL then a new Connection is created on which
    # the link will be attached. If a path is specified, but not the source
    # address, then the path of the URL is used as the target address.
    #
    # The name will be generated for the link if one is not specified.
    #
    # @param context [Connection, URL, String] The connection or the address.
    # @param options [Hash] Additional otpions.
    # @option options [String, Qpid::Proton::URL] The source address.
    # @option options [String] :target The target address
    # @option options [String] :name The link name.
    # @option options [Boolean] :dynamic
    # @option options [Object] :handler
    # @option options [Hash] :options Additional link options.
    #
    # @return [Receiver
    #
    def create_receiver(context, options = {})
      if context.is_a?(::String)
        context = Qpid::Proton::URL.new(context)
      end
      if context.is_a?(Qpid::Proton::URL)
        options[:source] ||= context.path # FIXME aconway 2016-01-14: modifying options?
      end
      self._session(context).open_receiver(options)

      # FIXME aconway 2016-01-14:
      # receiver.source.address = source if source
      # receiver.source.dynamic = true if options.has_key?(:dynamic) && options[:dynamic]
      # receiver.target.address = options[:target] if !options[:target].nil?
      # receiver.handler = options[:handler] if !options[:handler].nil?
      # self._apply_link_options(options[:options], receiver)
      # receiver.open
    end

    def declare_transaction(context, handler = nil, settle_before_discharge = false)
      if context.respond_to? :txn_ctl && !context.__send__(:txn_ctl).nil?
        class << context
          attr_accessor :txn_ctl
        end
        context.txn_ctl = self.create_sender(context, nil, "txn-ctl",
        InternalTransactionHandler.new())
      end
      return Transaction.new(context.txn_ctl, handler, settle_before_discharge)
    end

    # Initiates a server socket, accepting incoming AMQP connections on the
    # interface and port specified.
    #
    # @param url []
    # @param ssl_domain []
    #
    def listen(url, ssl_domain = nil)
      url = Qpid::Proton::URL.new(url)
      acceptor = self.acceptor(url.host, url.port)
      ssl_config = ssl_domain
      if ssl_config.nil? && (url.scheme == 'amqps') && @ssl
        ssl_config = @ssl.server
      end
      if !ssl_config.nil?
        acceptor.ssl_domain(ssl_config)
      end
      return acceptor
    end

    def do_work(timeout = nil)
      self.timeout = timeout unless timeout.nil?
      self.process
    end

    # FIXME aconway 2016-01-14: HERE
    def _apply_link_options(options, link)
      if !options.nil?
        if !options.is_a?(Enumerable)
          options = [Options].flatten
        end
        options.each {|o| o.apply(link) if o.test(link)}
      end
    end

    def to_s
      "#{self.class}<@impl=#{Cproton.pni_address_of(@impl)}>"
    end

  end

end
