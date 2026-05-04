.. _state_machine:

########################
Connection State Machine
########################

A Teleport session passes through four phases: Discovery, Signaling, Handshake, and Streaming. Each phase has well-defined entry and exit conditions, and each side runs its own state machine driven by the WebSocket and (later) the WebRTC connection state.

.. mermaid::

    stateDiagram-v2
        [*] --> Discovery
        Discovery --> Signaling : WebSocket open
        Signaling --> Handshake : SetupCommand received
        Handshake --> Streaming : AcknowledgeHandshake received
        Streaming --> Disconnecting : ShutdownCommand or transport loss
        Disconnecting --> [*]
        Signaling --> [*] : connect rejected
        Handshake --> [*] : timeout
        Streaming --> Streaming : ReconfigureVideo / SetupInputs / SetupLighting

Phases
======

Discovery
---------

The client locates a server by some out-of-band mechanism (manually-typed URL, DNS lookup, browser navigation, etc.) and resolves it to ``wss://host[:port]/``. The reference client's ``SessionClient`` opens a WebSocket; the reference ``SignalingService`` listens on the port given by ``InitialiseState::signalingPorts``.

Signaling
---------

The WebSocket is open. The first JSON frame the client sends is ``connect``, carrying its ``identity`` and ``client_id``. The server may close the WebSocket at this point if the identity is rejected. See :doc:`signaling` for the message catalogue.

Once the server accepts the connection, the WebRTC SDP exchange begins (``offer`` / ``answer`` / ``candidate``). When all six SCTP data channels have opened, the server transmits the binary :cpp:struct:`teleport::core::Handshake` frame.

Handshake
---------

The client receives ``Handshake`` and responds with a ``SetupCommand`` (server-to-client) followed by the supporting commands needed to render: ``SetupLightingCommand``, ``SetupInputsCommand``, ``SetOriginNodeCommand``, possibly ``UpdateNodeStructureCommand`` for the visible-node set, etc. The client confirms each ``AckedCommand`` with an ``AcknowledgementMessage`` and replies to the ``Handshake`` with an ``AcknowledgeHandshakeCommand`` carrying the visible-node set.

The handshake is complete when both sides have processed the ``AcknowledgeHandshake``.

Streaming
---------

In Streaming, all six data channels carry traffic continuously:

* video and video tags from server to client,
* geometry from server to client (with HTTP fallback for textures and meshes),
* audio in both directions (if ``audio_input_enabled`` is set),
* commands on the reliable channel,
* per-frame state on the unreliable channel.

The server may at any time issue ``ReconfigureVideoCommand``, ``SetupInputsCommand``, ``SetupLightingCommand``, ``SetOriginNodeCommand``, ``UpdateNodeStructureCommand`` or ``UpdateNodeSubtypeCommand``. None of these returns the session to an earlier phase; they are in-band reconfiguration.

The client continuously sends ``NodeStatusMessage``, ``ReceivedResourcesMessage``, ``InputStatesMessage``, ``InputEventsMessage`` and the per-controller pose stream defined in :doc:`service/client_to_server`.

Disconnect
----------

Either side may initiate disconnect:

* The server sends a ``ShutdownCommand`` (``CommandPayloadType::Shutdown``) and closes the data channels.
* The client sends a ``disconnect`` JSON frame on the WebSocket and closes its end of the WebRTC peer connection.
* The transport itself failing (WebSocket close, ICE failure, SCTP close) is treated as an implicit disconnect by both sides.

After disconnect the client should drop its ``GeometryCache`` for the disconnected server (or keep it as a warm cache, as the reference client does, keyed by ``session_id``) and return to Discovery.

Reconnection
============

The protocol does not currently define mid-session reconnection. A new ``Handshake`` always begins a new logical session.

* If the server returns the same ``session_id`` in the next ``SetupCommand``, the client may reuse cached geometry/textures: every ``ReceivedResourcesMessage`` it has already sent for those uids is still valid.
* If ``session_id`` differs, the client must treat the new session as unrelated and re-acknowledge resources from scratch.

Connection state enumeration
============================

The reference client exposes the underlying transport state through :cpp:enum:`avs::StreamingConnectionState`:

.. list-table::
   :widths: 10 30
   :header-rows: 1

   * - Value
     - Meaning
   * - ``UNINITIALIZED``
     - The pipeline has not been configured.
   * - ``NEW_UNCONNECTED``
     - WebSocket open, awaiting WebRTC negotiation.
   * - ``CONNECTING``
     - Performing the SDP/ICE exchange.
   * - ``CONNECTED``
     - All six data channels are open; ``Handshake`` may be exchanged.
   * - ``DISCONNECTED``
     - Clean shutdown.
   * - ``FAILED``
     - The transport reported an unrecoverable error.
   * - ``CLOSED``
     - The WebSocket was closed by the peer.
   * - ``ERROR_STATE``
     - Internal error in the local pipeline.

Timeouts
========

* Signaling: implementation-defined; the reference client treats a 30 s WebSocket connect failure as a hard error.
* Handshake: ``SetupCommand.idle_connection_timeout`` (default 5000 ms) is the inactivity timeout the server expects the client to enforce on the data transport once it is open.
* Streaming: the same ``idle_connection_timeout`` applies; absence of any traffic on the reliable channel for that duration is grounds for disconnect.
