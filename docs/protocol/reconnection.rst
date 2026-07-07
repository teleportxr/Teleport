.. _reconnection:

############
Reconnection
############

When an established Teleport session loses its real-time transport (the WebRTC
peer connection drops, the WebSocket closes, or the underlying network goes
away) the client does not immediately tear down its local reflection of the
remote scene. Instead it tries a bounded number of times to re-establish the
session with the same server, and only gives up — clearing the geometry cache
and stopping rendering of the remote content — when those attempts have all
failed.

Triggering a reconnect
======================

The reference client samples ``avs::StreamingConnectionState`` (exposed by
:cpp:class:`avs::WebRtcNetworkSource`) once per frame. A transition from
``CONNECTED`` to any of ``DISCONNECTED``, ``FAILED`` or ``CLOSED`` is treated
as a drop of an established session and triggers reconnection, but only if the
client had previously completed a setup handshake (``lastSessionId`` is
non-zero); an initial-connect failure is left to the user to retry.

State machine
=============

.. mermaid::

    stateDiagram-v2
        [*] --> UNCONNECTED
        UNCONNECTED --> OFFERING : user requests connection
        OFFERING --> AWAITING_SETUP : connect-response + Connect()
        AWAITING_SETUP --> HANDSHAKING : SetupCommand received
        HANDSHAKING --> CONNECTED : AcknowledgeHandshake received
        CONNECTED --> RECONNECTING : streaming state -> DISCONNECTED/FAILED/CLOSED
        RECONNECTING --> RECONNECTING : back-off elapsed, attempt fails
        RECONNECTING --> AWAITING_SETUP : connect-response received
        RECONNECTING --> UNCONNECTED : max attempts exhausted (give up)
        CONNECTED --> UNCONNECTED : explicit disconnect

The new ``RECONNECTING`` value is added to
:cpp:enum:`teleport::client::ConnectionStatus`. While the client is in
``RECONNECTING`` it does not send a ``disconnect`` JSON frame to the server —
it keeps the previous ``clientID`` and ``serverID`` so that the server can
match the next ``connect`` as a continuation of the session.

Back-off
========

Each time the per-attempt deadline (``Options::connectionTimeout``) elapses
without a ``connect-response``, the back-off doubles, clamped to
``Options::reconnectMaxBackoffMs``. The first wait is
``Options::reconnectInitialBackoffMs``. After
``Options::maxReconnectAttempts`` failed attempts the client gives up.

Configuration
=============

The reconnection behaviour is controlled by three fields on
:cpp:struct:`teleport::client::Options`, persisted in ``options.txt`` under the
keys ``MaxReconnectAttempts``, ``ReconnectInitialBackoffMs`` and
``ReconnectMaxBackoffMs``:

.. list-table::
   :widths: 30 10 60
   :header-rows: 1

   * - Field
     - Default
     - Meaning
   * - ``maxReconnectAttempts``
     - ``4``
     - Number of attempts before giving up. Set to ``0`` to disable
       reconnection (the client will give up immediately on a drop).
   * - ``reconnectInitialBackoffMs``
     - ``500``
     - Wait before the first attempt, in milliseconds.
   * - ``reconnectMaxBackoffMs``
     - ``4000``
     - Upper bound for the back-off, in milliseconds. Each subsequent attempt
       doubles the previous wait and is clamped to this value.

Server-ID matching
==================

A reconnect is only treated as a *continuation* of the previous session when
the ``serverID`` returned in the ``connect-response`` matches the one the
client recorded for the previous successful connection.

* If ``clientID`` and ``serverID`` both match the values the client already
  holds, :cpp:class:`teleport::client::SignalingServer` clears its
  ``clearResources`` flag. ``SessionClient::HandleConnections`` then preserves
  the geometry cache, and the next ``SetupCommand`` arriving with the same
  ``session_id`` allows the client to skip re-acknowledging resources it has
  already cached.
* If either id differs (for example because the server application has
  restarted and considers itself to be running a new state),
  ``SignalingServer`` adopts the new ids and signals
  ``DiscoveryService::ShouldClear``. ``SessionClient`` then drops the
  geometry cache before completing the new setup, exactly as for a first-time
  connection to a fresh server.

Giving up
=========

When the maximum number of attempts is exhausted, the client:

1. Tears down the streaming pipeline
   (``ClientPipeline::pipeline.deconfigure()`` and
   ``source->deconfigure()``).
2. Calls :cpp:func:`teleport::client::SessionCommandInterface::OnReconnectGaveUp`
   on the renderer. The reference :cpp:class:`teleport::clientrender::InstanceRenderer`
   responds by closing the video stream and clearing the geometry cache, so
   the local reflection of the remote scene is no longer drawn.
3. Clears the geometry cache associated with the session.
4. Performs a clean :cpp:func:`teleport::client::SessionClient::Disconnect`,
   resetting ``clientID`` and sending a ``disconnect`` frame to the server (if
   the WebSocket is still open).
5. Returns to ``UNCONNECTED``. A subsequent user action (clicking a bookmark,
   re-typing the URL) is needed to start a new session.

Surfacing transport drops
=========================

To make reconnection responsive, the WebRTC network source pushes its state to
``DISCONNECTED`` as soon as a ``send`` call fails because the channel is
closed, in addition to the normal ``onStateChange`` callback. This guarantees
that the next ``Frame`` call observes the drop without waiting for the peer
connection's own state machine to catch up.
