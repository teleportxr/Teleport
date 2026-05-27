.. _client_to_server:

Messages from Client to Server
##############################

The client emits messages over two WebRTC data channels:

* the **reliable** channel (id 100) for ``Acknowledgement``, ``OrthogonalAcknowledgement``, ``ReceivedResources``, ``ResourceLost``, ``NodeStatus``, ``DisplayInfo`` and ``Handshake`` traffic that must not be lost;
* the **unreliable** channel (id 120) for per-frame messages: ``ControllerPoses``, ``InputStates``, ``InputEvents``, ``KeyframeRequest`` and ``PongForLatency``.

Any **reliable**-channel message MAY instead be sent as a binary frame on the signaling WebSocket. The payload bytes are identical, and a client MUST use this fallback transport whenever the WebRTC ``reliable`` data channel is not yet ``open`` (notably for the initial ``Handshake``). Servers MUST accept reliable messages on either transport for the lifetime of the session; see :ref:`signaling_reliable_fallback`. The ``unreliable`` channel has no such fallback.

The **Client** sends all of its messages in its local units and :ref:`AxesStandard <conventions>`. The **Server** is responsible for converting incoming data to its own internal representation, and for converting outgoing commands to the client's units.

.. _client_message_header_full:

Common header
=============

Every client-to-server message begins with the 9-byte ``ClientMessage`` header (reference: ``teleport::core::ClientMessage``):

.. list-table:: ClientMessage header
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - ClientMessagePayloadType
     - The message type (see table below).
   * - 8
     - int64
     - ``timestamp_session_us`` -- microseconds elapsed on the client since its local session start when this message was constructed.

.. list-table:: ClientMessagePayloadType
   :widths: 8 30 50
   :header-rows: 1

   * - Id
     - Name
     - Reference struct
   * - 0
     - ``Invalid``
     - (never sent)
   * - 1
     - ``Handshake``
     - ``teleport::core::Handshake`` (initial send normally goes on the signaling WebSocket via the reliable-channel fallback, see :ref:`handshake` and :ref:`signaling_reliable_fallback`)
   * - 2
     - ``NodeStatus``
     - ``teleport::core::NodeStatusMessage``
   * - 3
     - ``ReceivedResources``
     - ``teleport::core::ReceivedResourcesMessage``
   * - 4
     - ``ControllerPoses``
     - ``teleport::core::NodePosesMessage``
   * - 5
     - ``ResourceLost``
     - ``teleport::core::ResourceLostMessage``
   * - 6
     - ``InputStates``
     - ``teleport::core::InputStatesMessage``
   * - 7
     - ``InputEvents``
     - ``teleport::core::InputEventsMessage``
   * - 8
     - ``DisplayInfo``
     - ``teleport::core::DisplayInfoMessage``
   * - 9
     - ``KeyframeRequest``
     - ``teleport::core::KeyframeRequestMessage``
   * - 10
     - ``PongForLatency``
     - ``teleport::core::PongForLatencyMessage``
   * - 11
     - ``OrthogonalAcknowledgement``
     - ``teleport::core::OrthogonalAcknowledgementMessage``
   * - 12
     - ``Acknowledgement``
     - ``teleport::core::AcknowledgementMessage``

DisplayInfo (id = 8)
====================

The byte layout (reference: ``teleport::core::DisplayInfoMessage``):

.. list-table:: DisplayInfoMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``DisplayInfo``, timestamp).
   * - 4
     - uint32
     - ``width``
   * - 4
     - uint32
     - ``height``
   * - 4
     - float
     - ``framerate`` (Hz, measured).

ControllerPoses (id = 4)
========================

Sent on the **unreliable** channel (reference: ``teleport::core::NodePosesMessage``).

.. list-table:: NodePosesMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``ControllerPoses``, timestamp).
   * - 28
     - Pose_packed
     - ``headPose`` (vec4 orientation + vec3 position).
   * - 2
     - uint16
     - ``numPoses`` = N.
   * - N × 36
     - NodePose[]
     - For each pose: 8-byte node uid, 28-byte ``PoseDynamic_packed`` (pose + linear velocity + angular velocity), all in stage space.

ReceivedResources (id = 3)
==========================

Sent on the **reliable** channel (reference: ``teleport::core::ReceivedResourcesMessage``).

.. list-table:: ReceivedResourcesMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``ReceivedResources``, timestamp).
   * - 8
     - size_t
     - ``receivedResourcesCount`` = N.
   * - 8 * N
     - uid[]
     - Resource uids that have been fully decoded and stored.

NodeStatus (id = 2)
===================

Sent on the **reliable** channel to tell the server which nodes the client is currently rendering, and which it has chosen to release (reference: ``teleport::core::NodeStatusMessage``).

.. list-table:: NodeStatusMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``NodeStatus``, timestamp).
   * - 8
     - size_t
     - ``nodesDrawnCount`` = N.
   * - 8
     - size_t
     - ``nodesWantToReleaseCount`` = M.
   * - 8 * N
     - uid[]
     - Drawn node uids.
   * - 8 * M
     - uid[]
     - Node uids the client wants to release.

ResourceLost (id = 5)
=====================

Sent on the **reliable** channel to tell the server that a previously-confirmed resource has been lost (e.g. due to a decoder error) and must be re-sent (reference: ``teleport::core::ResourceLostMessage``).

.. list-table:: ResourceLostMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``ResourceLost``, timestamp).
   * - 2
     - uint16
     - ``resourceCount`` = N.
   * - 8 * N
     - uid[]
     - Lost resource uids.

InputStates (id = 6)
====================

Sent on the **unreliable** channel each frame, after :ref:`SetupInputs <server_to_client>` has been received. Carries the current value of every state-typed input declared by the server (reference: ``teleport::core::InputStatesMessage``). See :doc:`../input` for the full input model.

.. list-table:: InputStatesMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``InputStates``, timestamp).
   * - 2
     - uint16
     - ``numBinaryStates`` = B.
   * - 2
     - uint16
     - ``numAnalogueStates`` = A.
   * - B (bits, padded to bytes)
     - bitfield
     - One bit per declared binary-state input, in declaration order.
   * - 4 * A
     - float[]
     - One float per declared analogue-state input, in declaration order.

InputEvents (id = 7)
====================

Sent on the **unreliable** channel when one or more event-typed inputs have fired since the last frame (reference: ``teleport::core::InputEventsMessage``).

.. list-table:: InputEventsMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``InputEvents``, timestamp).
   * - 2
     - uint16
     - ``numBinaryEvents`` = B.
   * - 2
     - uint16
     - ``numAnalogueEvents`` = A.
   * - 2
     - uint16
     - ``numMotionEvents`` = M.
   * - B × 7
     - InputEventBinary[]
     - 4-byte ``eventID``, 2-byte ``inputID``, 1-byte ``activated`` flag.
   * - A × 10
     - InputEventAnalogue[]
     - 4-byte ``eventID``, 2-byte ``inputID``, 4-byte ``strength`` (float).
   * - M × 14
     - InputEventMotion[]
     - 4-byte ``eventID``, 2-byte ``inputID``, 8-byte ``vec2`` motion.

KeyframeRequest (id = 9)
========================

Sent on the **unreliable** channel when the video decoder has lost sync; the server forces the encoder to emit the next frame as an IDR (reference: ``teleport::core::KeyframeRequestMessage``).

.. list-table:: KeyframeRequestMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``KeyframeRequest``, timestamp). No additional fields.

PongForLatency (id = 10)
========================

Sent on the **unreliable** channel in response to :ref:`PingForLatency <server_to_client>`. The server uses the round-trip to estimate one-way latency (reference: ``teleport::core::PongForLatencyMessage``).

.. list-table:: PongForLatencyMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``PongForLatency``, timestamp).
   * - 8
     - int64
     - ``unix_time_us`` -- the value echoed from the matching ``PingForLatencyCommand``.
   * - 8
     - int64
     - ``server_to_client_latency_us`` -- the client's measured one-way latency from server to client (microseconds).

Acknowledgement (id = 12)
=========================

Sent on the **reliable** channel in response to any :ref:`AckedCommand <server_to_client>` (currently ``SetupLighting`` and ``SetOriginNode``; reference: ``teleport::core::AcknowledgementMessage``).

.. list-table:: AcknowledgementMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``Acknowledgement``, timestamp).
   * - 8
     - uint64
     - ``ack_id`` -- copied from the command being acknowledged.

OrthogonalAcknowledgement (id = 11)
===================================

Sent on the **reliable** channel to confirm a single ``confirmationNumber`` carried by a ``NodeStateCommand`` (e.g. ``UpdateNodeStructure``). Allows independent state updates to be confirmed without the monotonic ``ack_id`` ordering of ``AckedCommand`` (reference: ``teleport::core::OrthogonalAcknowledgementMessage``).

.. list-table:: OrthogonalAcknowledgementMessage
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 9
     - ClientMessage
     - Standard header (``OrthogonalAcknowledgement``, timestamp).
   * - 8
     - uint64
     - ``confirmationNumber``.
