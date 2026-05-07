.. _server_to_client:

Commands from Server to Client
##############################

The **Server** sends commands to the **Client** over the **reliable** WebRTC data channel (id 100, label ``reliable``). All commands are little-endian, packed (no padding) C structs whose first byte is a :cpp:enum:`teleport::core::CommandPayloadType` discriminator. Coordinates and units are converted on the server to the client's :ref:`AxesStandard <conventions>` before sending.

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
     - :cpp:struct:`teleport::core::ShutdownCommand`
     - none
   * - 2
     - ``Setup``
     - :cpp:struct:`teleport::core::SetupCommand`
     - none (154 bytes; see :doc:`../service`)
   * - 3
     - ``AcknowledgeHandshake``
     - :cpp:struct:`teleport::core::AcknowledgeHandshakeCommand`
     - ``visibleNodeCount`` × ``uid``
   * - 4
     - ``ReconfigureVideo``
     - :cpp:struct:`teleport::core::ReconfigureVideoCommand`
     - none (carries a fresh :cpp:struct:`avs::VideoConfig`)
   * - 5
     - ``NodeVisibility``
     - :cpp:struct:`teleport::core::NodeVisibilityCommand`
     - ``nodesShowCount`` + ``nodesHideCount`` uids
   * - 6
     - ``UpdateNodeMovement``
     - :cpp:struct:`teleport::core::UpdateNodeMovementCommand`
     - ``updatesCount`` × :cpp:struct:`teleport::core::MovementUpdate`
   * - 7
     - ``UpdateNodeEnabledState``
     - :cpp:struct:`teleport::core::UpdateNodeEnabledStateCommand`
     - ``updatesCount`` × :cpp:struct:`teleport::core::NodeUpdateEnabledState`
   * - 8
     - ``SetNodeHighlighted``
     - :cpp:struct:`teleport::core::SetNodeHighlightedCommand`
     - none
   * - 9
     - ``ApplyNodeAnimation``
     - :cpp:struct:`teleport::core::ApplyAnimationCommand`
     - none (embeds :cpp:struct:`teleport::core::ApplyAnimation`)
   * - 10
     - ``UpdateNodeAnimationControlX``
     - (reserved)
     - reserved
   * - 11
     - ``SetNodeAnimationSpeed``
     - :cpp:struct:`teleport::core::SetNodeAnimationSpeedCommand`
     - none
   * - 12
     - ``SetupLighting``
     - :cpp:struct:`teleport::core::SetLightingCommand`
     - ``num_gi_textures`` × ``uid`` (acked; see ``ack_id``)
   * - 13
     - ``UpdateNodeStructure``
     - :cpp:struct:`teleport::core::UpdateNodeStructureCommand`
     - none
   * - 14
     - ``AssignNodePosePath``
     - :cpp:struct:`teleport::core::AssignNodePosePathCommand`
     - ``pathLength`` UTF-8 bytes
   * - 15
     - ``SetupInputs``
     - :cpp:struct:`teleport::core::SetupInputsCommand`
     - ``numInputs`` × :cpp:struct:`teleport::core::InputDefinitionNetPacket` (each followed by ``pathLength`` UTF-8 bytes)
   * - 16
     - ``PingForLatency``
     - :cpp:struct:`teleport::core::PingForLatencyCommand`
     - none (sent over the unreliable channel; client replies with :ref:`PongForLatency <client_to_server>`)
   * - 128
     - ``SetOriginNode``
     - :cpp:struct:`teleport::core::SetOriginNodeCommand`
     - none (acked; see ``ack_id``)

Acknowledged commands
=====================

Commands derived from :cpp:struct:`teleport::core::AckedCommand` (currently ``SetupLighting`` and ``SetOriginNode``) carry an additional ``uint64_t ack_id`` field after the 1-byte type. The client must reply with an :ref:`AcknowledgementMessage <client_to_server>` containing the same ``ack_id``. ``ack_id`` increases monotonically per session; clients can ignore any id less than or equal to one already received.

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
     - New video configuration (same layout as inside :cpp:struct:`teleport::core::SetupCommand`).

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
   * - N * sizeof(MovementUpdate)
     - MovementUpdate[]
     - Per-node motion updates.

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
     - ``cacheID``
   * - 8
     - uid
     - ``animationID``
   * - 4
     - float
     - ``animTimeAtTimestamp`` -- where in the animation we should be at ``timestampUs``.
   * - 4
     - float
     - ``speedUnitsPerSecond``
   * - 1
     - bool
     - ``loop``

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
     - Dynamic lighting parameters (specular/diffuse/light positions, sizes, mips, mode, two cubemap uids). See :cpp:struct:`teleport::core::ClientDynamicLighting`.

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
