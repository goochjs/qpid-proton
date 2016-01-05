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

module Qpid::Proton

  # A session is the parent for senders and receivers.
  #
  # A Session has a single parent Qpid::Proton::Connection instance.
  #
  class Session < Endpoint

    # @private
    include Util::Wrapper

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_session"

    # @!attribute incoming_capacity
    #
    # The incoming capacity of a session determines how much incoming message
    # data the session will buffer. Note that if this value is less than the
    # negotatied frame size of the transport, it will be rounded up to one full
    # frame.
    #
    # @return [Fixnum] The incoing capacity of the session, measured in bytes.
    #
    proton_accessor :incoming_capacity

    # @private
    proton_reader :attachments

    # @!attribute [r] outgoing_bytes
    #
    # @return [Fixnum] The number of outgoing bytes currently being buffered.
    #
    proton_caller :outgoing_bytes

    # @!attribute [r] incoming_bytes
    #
    # @return [Fixnum] The number of incomign bytes currently being buffered.
    #
    proton_caller :incoming_bytes

    # @!method open
    # Opens the session.
    #
    # Once this operaton has completed, the state flag is updated.
    #
    # @see LOCAL_ACTIVE
    #
    proton_caller :open

    # @!attribute [r] state
    #
    # @return [Fixnum] The endpoint state.
    #
    proton_caller :state

    # @private
    def self.wrap(impl)
      return nil if impl.nil?
      self.fetch_instance(impl, :pn_session_attachments) || Session.new(impl)
    end

    # @private
    def initialize(impl)
      @impl = impl
      self.class.store_instance(self, :pn_session_attachments)
    end

    # Closed the session.
    #
    # Once this operation has completed, the state flag will be set. This may be
    # called without calling #open, in which case it is the equivalence of
    # calling #open and then close immediately.
    #
    def close
      self._update_condition
      Cproton.pn_session_close(@impl)
    end

    # Retrieves the next session from a given connection that matches the
    # specified state mask.
    #
    # When uses with Connection#session_head an application can access all of
    # the session son the connection that match the given state.
    #
    # @param state_mask [Fixnum] The state mask to match.
    #
    # @return [Session, nil] The next session if one matches, or nil.
    #
    def next(state_mask)
      Session.wrap(Cproton.pn_session_next(@impl, state_mask))
    end

    # Returns the parent connection.
    #
    # @return [Connection] The connection.
    #
    def connection
      Connection.wrap(Cproton.pn_session_connection(@impl))
    end

    # Constructs a new sender.
    #
    # Each sender between two AMQP containers must be uniquely named. Note that
    # this uniqueness cannot be enforced at the library level, so some
    # consideration should be taken in choosing link names.
    #
    # @param name [String] The link name.
    #
    # @return [Sender, nil] The sender, or nil if an error occurred.
    #
    def sender(name)
      # FIXME aconway 2016-01-04: rename create_sender or remove?
      # Consistent use of create/open/plain name for all classes (session, sender, receiver)
      Sender.new(Cproton.pn_sender(@impl, name))
    end

    # Constructs a new receiver.
    #
    # Each receiver between two AMQP containers must be uniquely named. Note
    # that this uniqueness cannot be enforced at the library level, so some
    # consideration should be taken in choosing link names.
    #
    # @param name [String] The link name.
    #
    # @return [Receiver, nil] The receiver, or nil if an error occurred.
    #
    def receiver(name)
      # FIXME aconway 2016-01-04: rename create_sender or remove?
      # Consistent use of create/open/plain name for all classes (session, sender, receiver)
      Receiver.new(Cproton.pn_receiver(@impl, name))
    end

    # FIXME aconway 2016-01-04: doc opts or source
    def open_receiver(opts = {})
      opts = { :source => opts } if opts.is_a? String
      receiver = receiver(opts[:name] || SecureRandom.uuid)
      receiver.source.address ||= opts[:source]
      receiver.target.address ||= opts[:target]
      receiver.source.dynamic = true if opts[:dynamic]
      # FIXME aconway 2015-12-02: separate handlers per link?
      # FIXME aconway 2015-12-02: link options
      receiver.open
      return receiver
    end

    # FIXME aconway 2016-01-04: doc opts or target
    def open_sender(opts = {})
      opts = { :target => opts } if opts.is_a? String
      # FIXME aconway 2015-12-02: link IDs.
      sender = sender(opts[:name] || SecureRandom.uuid)
      sender.target.address ||= opts[:target]
      sender.source.address ||= opts[:source]
      sender.target.dynamic = true if opts[:dynamic]
      # FIXME aconway 2015-12-02: separate handlers per link?
      # FIXME aconway 2015-12-02: link options
      sender.open
      return sender
    end

    # @private
    def _local_condition
      Cproton.pn_session_condition(@impl)
    end

    # @private
    def _remote_condition # :nodoc:
      Cproton.pn_session_remote_condition(@impl)
    end

  end

end
