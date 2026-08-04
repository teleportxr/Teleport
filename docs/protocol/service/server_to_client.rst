.. _server_to_client:

Commands from Server to Client
##############################

The **Server** sends commands to the **Client** over the **reliable** WebRTC data channel (id 100, label ``reliable``). All commands are little-endian, packed (no padding) C structs whose first byte is a ``CommandPayloadType`` (a ``uint8_t``; reference enum: ``teleport::core::CommandPayloadType``) discriminator. The complete set of values is in the table below. Coordinates and units are converted on the server to the client's :ref:`AxesStandard <conventions>` before sending.

Any command described on this page MAY instead be delivered as a binary frame on the signaling WebSocket — the payload bytes are identical, and the server uses this fallback transport whenever the WebRTC ``reliable`` data channel is not yet ``open`` (notably for ``SetupCommand``). Receivers MUST accept commands on either transport for the lifetime of the session; see :ref:`signaling_reliable_fallback`.

The complete set of command types is enumerated below. Variable-length commands declare the count of the trailing array(s) inside the struct; the receiver reads ``sizeof(StructType)`` bytes followed by the trailing data.

.. list-table:: CommandPayloadType
   :widths: 8 30 12 50
   :header-rows: 1

   * - Id
     - Name
     - Reference struct
     - Trailing data
   * - 0
     - ``Invalid``
     -
     - (never sent)
   * - 1
     - ``Shutdown``
     - ``teleport::core::ShutdownCommand``
     - none
   * - 2
     - ``Setup``
     - ``teleport::core::SetupCommand``
     - none (154 bytes; see :doc:`../service`)
   * - 3
     - ``AcknowledgeHandshake``
     - ``teleport::core::AcknowledgeHandshakeCommand``
     - ``visibleNodeCount`` × ``uid``
   * - 4
     - ``ReconfigureVideo``
     - ``teleport::core::ReconfigureVideoCommand``
     - none (carries a fresh ``avs::VideoConfig``)
   * - 5
     - ``NodeVisibility``
     - ``teleport::core::NodeVisibilityCommand``
     - ``nodesShowCount`` + ``nodesHideCount`` uids
   * - 6
     - ``UpdateNodeMovement``
     - ``teleport::core::UpdateNodeMovementCommand``
     - ``updatesCount`` × ``teleport::core::MovementUpdate``
   * - 7
     - ``UpdateNodeEnabledState``
     - ``teleport::core::UpdateNodeEnabledStateCommand``
     - ``updatesCount`` × ``teleport::core::NodeUpdateEnabledState``
   * - 8
     - ``SetNodeHighlighted``
     - ``teleport::core::SetNodeHighlightedCommand``
     - none
   * - 9
     - ``ApplyNodeAnimation``
     - ``teleport::core::ApplyAnimationCommand``
     - none (embeds ``teleport::core::ApplyAnimation``)
   * - 10
     - ``Reserved_AnimationControl``
     - (reserved)
     - reserved -- never had a struct or a handler. The id is held so that those after it stay put.
   * - 11
     - ``SetNodeAnimationSpeed``
     - ``teleport::core::SetNodeAnimationSpeedCommand``
     - none -- **do not send**; see below.
   * - 12
     - ``SetupLighting``
     - ``teleport::core::SetLightingCommand``
     - ``num_gi_textures`` × ``uid`` (acked; see ``ack_id``)
   * - 13
     - ``UpdateNodeStructure``
     - ``teleport::core::UpdateNodeStructureCommand``
     - none
   * - 14
     - ``AssignNodePosePath``
     - ``teleport::core::AssignNodePosePathCommand``
     - ``pathLength`` UTF-8 bytes
   * - 15
     - ``SetupInputs``
     - ``teleport::core::SetupInputsCommand``
     - ``numInputs`` × ``teleport::core::InputDefinitionNetPacket`` (each followed by ``pathLength`` UTF-8 bytes)
   * - 16
     - ``PingForLatency``
     - ``teleport::core::PingForLatencyCommand``
     - none (sent over the unreliable channel; client replies with :ref:`PongForLatency <client_to_server>`)
   * - 17
     - ``AudioSourceMapping``
     - *(reserved)*
     - Deprecated; audio is bound by the :ref:`track mid = node uid <audio_node_binding>`. MUST NOT be sent.
   * - 18
     - ``AudioParticipantStateChange``
     - *(reserved)*
     - Deprecated; audio is bound by the :ref:`track mid = node uid <audio_node_binding>`. MUST NOT be sent.
   * - 128
     - ``SetOriginNode``
     - ``teleport::core::SetOriginNodeCommand``
     - none (acked; see ``ack_id``)

Acknowledged commands
=====================

Commands derived from ``AckedCommand`` (reference: ``teleport::core::AckedCommand``; currently ``SetupLighting`` and ``SetOriginNode``) carry an additional ``uint64_t ack_id`` field after the 1-byte type. The client must reply with an :ref:`AcknowledgementMessage <client_to_server>` containing the same ``ack_id``. ``ack_id`` increases monotonically per session; clients can ignore any id less than or equal to one already received.

Selected command layouts
========================

.. list-table:: ShutdownCommand (id = 1)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``Shutdown``

.. list-table:: ReconfigureVideoCommand (id = 4)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``ReconfigureVideo``
   * - 89
     - avs::VideoConfig
     - New video configuration (same layout as inside ``teleport::core::SetupCommand``).

.. list-table:: NodeVisibilityCommand (id = 5)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``NodeVisibility``
   * - 8
     - size_t
     - ``nodesShowCount`` = S
   * - 8
     - size_t
     - ``nodesHideCount`` = H
   * - 8 * S
     - uid[]
     - Nodes to show.
   * - 8 * H
     - uid[]
     - Nodes to hide.

.. list-table:: UpdateNodeMovementCommand (id = 6)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``UpdateNodeMovement``
   * - 8
     - size_t
     - ``updatesCount`` = N
   * - N * 85
     - MovementUpdate[]
     - Per-node motion updates.

Only nodes streamed with ``stationary`` = false will move: the client early-outs on
``isStatic`` in ``Node::TickExtrapolatedTransform``.

.. list-table:: MovementUpdate (85 bytes, packed)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 8
     - int64
     - ``server_time_us``, microseconds since ``SetupCommand.startTimestamp_utc_unix_us``.
   * - 1
     - bool
     - ``isGlobal``. False means the transform is local to the node's parent, which is what
       a node parented under a client's origin node requires.
   * - 8
     - uid
     - ``nodeID``.
   * - 12
     - vec3
     - ``position``, metres.
   * - 16
     - vec4
     - ``rotation``, quaternion (x, y, z, w).
   * - 12
     - vec3
     - ``scale``.
   * - 12
     - vec3
     - ``velocity``, metres per second, for extrapolation to predicted display time.
   * - 12
     - vec3
     - ``angularVelocityAxis``, unit axis.
   * - 4
     - float
     - ``angularVelocityAngle``, radians per second about that axis.

All values are already converted into the client's ``AxesStandard`` by the server.

.. note::
   Sending a non-zero ``velocity`` or ``angularVelocityAngle`` permanently enables a heavily
   damped smoothing filter on the client (``smoothingEnabled`` in
   ``ClientRender/Node.cpp``), which interpolates at roughly 0.0166 per frame and is never
   switched off again. Servers that update a node frequently should leave both zero.

.. list-table:: SetNodeHighlightedCommand (id = 8)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``SetNodeHighlighted``
   * - 8
     - uid
     - ``nodeID``
   * - 1
     - bool
     - ``isHighlighted``

.. list-table:: ApplyAnimationCommand (id = 9)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``ApplyNodeAnimation``
   * - 4
     - int32
     - ``animLayer``
   * - 8
     - int64
     - ``timestampUs`` -- when the state should apply, in server-session time (microseconds since ``SetupCommand.startTimestamp_utc_unix_us``).
   * - 8
     - uid
     - ``nodeID``
   * - 8
     - uid
     - ``cacheID`` -- which geometry cache holds the animation resource. **Zero means "the cache
       containing** ``nodeID`` **"**, which is what a server normally sends: caches are created
       client-side with client-local uids, so the server has no way to name one. A non-zero value
       that the client cannot resolve is warned about and the update dropped.
   * - 8
     - uid
     - ``animationID``
   * - 4
     - float
     - ``animTimeAtTimestamp`` -- where in the animation we should be at ``timestampUs``, in
       **seconds** from the start of the clip.
   * - 4
     - float
     - ``speedUnitsPerSecond`` -- a **playback-rate multiplier**, not metres per second. ``1.0``
       plays the clip at its authored rate, ``2.0`` at double speed. Use it to match a locomotion
       clip to the speed the node is actually moving at, which removes foot-sliding without
       needing more clips.
   * - 1
     - bool
     - ``loop``

Total 46 bytes. ``SessionClient::ReceiveNodeAnimationUpdate`` requires that size **exactly** and
silently drops any packet of another length.

.. note::
   ``timestampUs`` in the future is the mechanism for cross-fading: the client synthesises a
   snapshot of the current state at "now" and interpolates from it to the new state, so the lead
   time *is* the blend duration. A timestamp of "now" gives a hard snap.

.. note::
   ``animLayer`` must be ``0``. ``AnimationInstance::Update`` processes only layer 0; states
   written to any other layer are stored and never applied.

.. note::
   A node whose renderable content came from a URL (a glTF or VRM asset) holds an entire
   *sub-scene*, decoded into its own cache with client-local uids. Address ``nodeID`` at the outer
   node the server created; the client forwards the update to the skeletons inside. ``cacheID``
   still refers to the cache holding the **animation**, which is the outer one.

.. list-table:: SetNodeAnimationSpeedCommand (id = 11)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``SetNodeAnimationSpeed``
   * - 8
     - uid
     - ``nodeID``
   * - 8
     - uid
     - ``animationID``
   * - 4
     - float
     - ``speed``

.. warning::
   **Do not send this command.** ``SessionClient::ReceiveCommandPacket`` has no case for id 11 and
   falls through to its break-on-unknown default. The struct is documented because it is still
   defined, not because it works. Set the playback rate with ``ApplyAnimation.speedUnitsPerSecond``
   instead.

.. list-table:: SetLightingCommand (id = 12, acked)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``SetupLighting``
   * - 8
     - uint64
     - ``ack_id``
   * - 57
     - ClientDynamicLighting
     - Dynamic lighting parameters (specular/diffuse/light positions, sizes, mips, mode, two cubemap uids; reference: ``teleport::core::ClientDynamicLighting``).

.. list-table:: UpdateNodeStructureCommand (id = 13)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``UpdateNodeStructure``
   * - 8
     - uid
     - ``nodeID``
   * - 8
     - uint64
     - ``confirmationNumber``
   * - 8
     - uid
     - ``parentID``
   * - 28
     - Pose_packed
     - ``relativePose`` (vec4 orientation + vec3 position).

.. list-table:: AssignNodePosePathCommand (id = 14)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``AssignNodePosePath``
   * - 8
     - uid
     - ``nodeID``
   * - 2
     - uint16
     - ``pathLength`` = P
   * - P
     - char[]
     - UTF-8 regular expression matching a client-side OpenXR pose path. If ``pathLength == 0``, control of the node is returned to the server.

.. list-table:: SetupInputsCommand (id = 15)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``SetupInputs``
   * - 2
     - uint16
     - ``numInputs`` = I
   * - I × variable
     - InputDefinitionNetPacket[]
     - Each definition is 5 bytes (``InputId`` + ``InputType`` + ``pathLength``) followed by ``pathLength`` UTF-8 bytes. See :doc:`../input`.

.. list-table:: PingForLatencyCommand (id = 16)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``PingForLatency``
   * - 8
     - int64
     - ``unix_time_us`` -- the server's UTC clock when the ping was sent, in microseconds. The client echoes it in :ref:`PongForLatencyMessage <client_to_server>`.

.. list-table:: SetOriginNodeCommand (id = 128, acked)
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``SetOriginNode``
   * - 8
     - uint64
     - ``ack_id``
   * - 8
     - uid
     - ``origin_node`` -- session uid of the node to use as the client's stage-space origin.
   * - 8
     - uint64
     - ``valid_counter`` -- monotonic; ignore messages with smaller values than the last received.
