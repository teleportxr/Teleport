############
Data Service
############

Initialization
^^^^^^^^^^^^^^
After a **Connecting Client** completes signaling, the Data Service has been created. This connection has six logical streams (channels). Each is identified on the wire by a numeric **stream id** that is the same on the server and the client.

.. list-table:: Data Service Channels
   :widths: 8 14 7 7 7 24
   :header-rows: 1

   * - Stream id
     - Label
     - Reliable
     - Ordered
     - Two-way
     - Description
   * - 20
     - ``video``
     - No
     - No
     - No
     - Video stream from server to client (parsed as AVC/HEVC Annex-B).
   * - 40
     - ``video_tags``
     - No
     - No
     - No
     - Per-frame video metadata; framed payloads.
   * - 60
     - ``audio_server_to_client``
     - No
     - No
     - Yes
     - Bidirectional audio; framed payloads.
   * - 80
     - ``geometry``
     - Yes
     - Yes
     - No
     - Geometry streaming to client; framed payloads with the parser described in :ref:`geometry_payload`.
   * - 100
     - ``reliable``
     - Yes
     - Yes
     - Yes
     - Reliable commands and messages, including all ``CommandPayloadType`` traffic and acknowledgements.
   * - 120
     - ``unreliable``
     - No
     - No
     - Yes
     - Unreliable, time-sensitive messages (per-frame poses, input states).

The reference server defines these streams in ``NetworkPipeline::initialise``; the reference client mirrors them in ``ClientPipeline::Init``. The labels above must match exactly across implementations; the numeric ids are conventional but recommended.

The video stream can be configured as H.264 or HEVC; see :ref:`video`.

When initial signaling is complete for this **Client**, the **Server** sends the **Setup Command** to the **Client** on the **reliable** channel (id 100).

*Definitions*

A **server session** starts when a server first initializes into its running state, and continues until the **Server** stops it.
A **client-server session** starts when a client first connects to a specific server address, and continues until either the Client or the Server decides it is finished. This may include multiple **network sessions**, for example in the case of network outages or connection being paused by either party. The lifetime of a **client-server session** is a subset of the **server session**.
A **uid** is an unsigned 64-bit number that is unique *for a specific Server Session*. **uid**'s are shared between the server and all of its clients. A **uid** need not be unique for different servers connected to the same client.

.. _command_packet:

Command packets (server-to-client)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

All packets on the **reliable** channel from server to client are commands. Each command begins with a one-byte ``CommandPayloadType`` discriminator and is followed by the fixed body of the command struct, then any appended variable-length data. The full command catalogue is in :doc:`service/server_to_client`.

+------------------------+--------------------+--------------------------------------------+
|          bytes         |       type         |      description                           |
+========================+====================+============================================+
|      1                 | CommandPayloadType | Discriminator (see below).                 |
+------------------------+--------------------+--------------------------------------------+
|      sizeof(struct)-1  | command body       | Packed, 1-byte aligned, little-endian.     |
+------------------------+--------------------+--------------------------------------------+
|      0+                | appended data      | Optional, command-specific (e.g. uid lists)|
+------------------------+--------------------+--------------------------------------------+

.. _client_message_header:

Message packets (client-to-server)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

All packets sent by the client over the Data Service share a common 9-byte ``ClientMessage`` header. The ``Handshake`` (described below) is the only client message sent over the **signaling** WebSocket; every other client message is sent over the **reliable** channel (id 100) or the **unreliable** channel (id 120).

+------------------------+--------------------------+-----------------------------------------+
|          bytes         |        type              |    description                          |
+========================+==========================+=========================================+
|      1                 | ClientMessagePayloadType | Discriminator (see below).              |
+------------------------+--------------------------+-----------------------------------------+
|      8                 | int64                    | ``timestamp_session_us`` --             |
|                        |                          | microseconds since the *client*'s       |
|                        |                          | note: in instantiation                  |
|                        |                          | session start (see :ref:`conventions`). |
+------------------------+--------------------------+-----------------------------------------+
|      0+                | message body             | Packed, 1-byte aligned, little-endian   |
+------------------------+--------------------------+-----------------------------------------+
|      0+                | appended data            | Optional, message-specific.             |
+------------------------+--------------------------+-----------------------------------------+

The full client-to-server message catalogue is in :doc:`service/client_to_server`.



.. _setup_command:

Setup Command
^^^^^^^^^^^^^

The **Setup Command** is the first command sent by the server, immediately after the signaling exchange completes. It tells the client how to configure its video decoder, what time-base the server is using, and which background to render. Its total size is 154 bytes (``static_assert(sizeof(SetupCommand) == 154)``).

Reference implementation: :cpp:struct:`teleport::core::SetupCommand`.

.. list-table:: SetupCommand
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``Setup`` (= 2).
   * - 4
     - uint32
     - ``debug_stream``
   * - 4
     - uint32
     - ``debug_network_packets``
   * - 4
     - int32
     - ``requiredLatencyMs``
   * - 4
     - uint32
     - ``idle_connection_timeout`` (ms)
   * - 8
     - uint64
     - ``session_id`` -- changes when the server session changes; clients that see the same id may keep cached resources.
   * - 89
     - VideoConfig
     - ``video_config`` (see below).
   * - 4
     - float
     - ``draw_distance`` (metres).
   * - 1
     - AxesStandard
     - ``axesStandard`` -- the server's axis convention; the server converts everything sent to the client into the client's standard.
   * - 1
     - uint8
     - ``audio_input_enabled`` (1 if the server accepts an audio stream from the client).
   * - 1
     - uint8 (bool)
     - ``using_ssl`` -- if non-zero, the client should fetch HTTP assets over HTTPS.
   * - 8
     - int64
     - ``startTimestamp_utc_unix_us`` -- UTC Unix microseconds at which the server session began. Used as the zero point for ``MovementUpdate.server_time_us``.
   * - 1
     - BackgroundMode
     - ``backgroundMode`` -- ``NONE`` (0), ``COLOUR`` (1), ``TEXTURE`` (2), or ``VIDEO`` (3).
   * - 16
     - vec4
     - ``backgroundColour`` -- used when ``backgroundMode`` is ``COLOUR``.
   * - 8
     - uid
     - ``backgroundTexture`` -- used when ``backgroundMode`` is ``TEXTURE``; resolved via the HTTP service.

VideoConfig
~~~~~~~~~~~

89 bytes (``static_assert(sizeof(VideoConfig) == 89)``). Reference implementation: :cpp:struct:`avs::VideoConfig`. Lighting cubemap layout is **not** in ``VideoConfig`` -- it is sent later via the ``SetupLighting`` command (see :doc:`service/server_to_client`).

.. list-table:: VideoConfig
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 4
     - uint32
     - ``video_width``
   * - 4
     - uint32
     - ``video_height``
   * - 4
     - uint32
     - ``depth_width``
   * - 4
     - uint32
     - ``depth_height``
   * - 4
     - uint32
     - ``perspective_width``
   * - 4
     - uint32
     - ``perspective_height``
   * - 4
     - float
     - ``perspective_fov`` (degrees).
   * - 4
     - float
     - ``nearClipPlane`` (metres).
   * - 4
     - uint32
     - ``webcam_width``
   * - 4
     - uint32
     - ``webcam_height``
   * - 4
     - int32
     - ``webcam_offset_x``
   * - 4
     - int32
     - ``webcam_offset_y``
   * - 4
     - uint32
     - ``use_10_bit_decoding`` (0 or 1).
   * - 4
     - uint32
     - ``use_yuv_444_decoding`` (0 or 1).
   * - 4
     - uint32
     - ``use_alpha_layer_decoding`` (0 or 1).
   * - 4
     - uint32
     - ``colour_cubemap_size``
   * - 4
     - int32
     - ``compose_cube``
   * - 4
     - int32
     - ``use_cubemap``
   * - 4
     - int32
     - ``stream_webcam``
   * - 4
     - avs::VideoCodec
     - ``videoCodec`` -- ``Any`` (0), ``H264``, ``HEVC``.
   * - 4
     - int32
     - ``shadowmap_x``
   * - 4
     - int32
     - ``shadowmap_y``
   * - 4
     - int32
     - ``shadowmap_size``

.. _handshake:

Handshake
^^^^^^^^^

After receiving the **Setup Command**, the client sends a ``Handshake`` message back to the server. In the reference implementation this single message is sent as a **binary WebSocket frame** over the signaling connection (see ``DiscoveryService::SendBinary``); all other client messages travel over the Data Service.

The ``Handshake`` is variable-size: it carries the fixed body below followed by ``resourceCount`` ``avs::uid`` entries listing the resources the client already has cached. Total fixed body size: 58 bytes (``static_assert(sizeof(Handshake) == 58)``). Reference implementation: :cpp:struct:`teleport::core::Handshake`.

It is a :ref:`ClientMessage <client_message_header>` and therefore begins with the standard 9-byte header.

.. list-table:: Handshake
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - ClientMessagePayloadType
     - ``Handshake`` (= 1).
   * - 8
     - int64
     - ``timestamp_session_us`` (see :ref:`client_message_header`).
   * - 4
     - uint32
     - ``startDisplayInfo.width``
   * - 4
     - uint32
     - ``startDisplayInfo.height``
   * - 4
     - float
     - ``startDisplayInfo.framerate`` (Hz, measured).
   * - 4
     - float
     - ``MetresPerUnit``
   * - 4
     - float
     - ``FOV`` (degrees).
   * - 4
     - uint32
     - ``udpBufferSize`` (kilobytes).
   * - 4
     - uint32
     - ``maxBandwidthKpS`` (kilobytes/s).
   * - 1
     - AxesStandard
     - ``axesStandard``
   * - 1
     - uint8
     - ``framerate`` (Hz, target).
   * - 1
     - bool
     - ``isVR``
   * - 8
     - uint64
     - ``resourceCount`` = N (number of cached resource uids that follow).
   * - 4
     - uint32
     - ``maxLightsSupported``
   * - 4
     - int32
     - ``minimumPriority`` -- meshes with lower priority need not be sent.
   * - 2
     - RenderingFeatures
     - ``renderingFeatures.normals`` (1 byte) + ``renderingFeatures.ambientOcclusion`` (1 byte).
   * - 8 * N
     - uid[]
     - Cached resource uids.

When the server receives the **Handshake**, it starts streaming data and sends an ``AcknowledgeHandshake`` command on the **reliable** channel (id 100):

.. list-table:: AcknowledgeHandshake
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``AcknowledgeHandshake`` (= 3).
   * - 8
     - size_t (uint64)
     - ``visibleNodeCount`` = N.
   * - 8 * N
     - uid[]
     - uids of nodes the client should expect to be streamed.

See :cpp:struct:`teleport::core::AcknowledgeHandshakeCommand`.

Once the **Client** has received the ``AcknowledgeHandshake``, initialization is complete and the client enters the main continuous Update mode.

Update
^^^^^^

.. toctree::
	:glob:
	:maxdepth: 2

	service/server_to_client
	service/client_to_server
