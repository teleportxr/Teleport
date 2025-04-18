############
Data Service
############

Initialization
^^^^^^^^^^^^^^
After a **Connecting Client** completes signaling, the Data Service has been created. This connection has six channels:

.. list-table:: Data Service Channels
   :widths: 15 7 7 30
   :header-rows: 1

   * - Name
     - Reliable
     - Two-way
     - Description
   * - Video
     - No
     - No
     - Video stream from server to client.
   * - Video Tags
     - No
     - No
     - Video per-frame metadata.
   * - Audio
     - No
     - Yes
     - Two-way audio channel.
   * - Geometry
     - No
     - No
     - Geometry streaming to client.
   * - Message
     - No
     - Yes
     - Unreliable, time-sensitive messages.
   * - Command
     - Yes
     - Yes
     - Reliable commands and responses.

The video stream can be configured as h264 or HEVC. The data streams 

When initial signaling is complete for this **Client**, the **Server** sends the **Setup Command** to the **Client** on the **Command** channel.

*Definitions*

A **server session** starts when a server first initializes into its running state, and continues until the **Server** stops it.
A **client-server session** starts when a client first connects to a specific server address, and continues until either the Client or the Server decides it is finished. This may include multiple **network sessions**, for example in the case of network outages or connection being paused by either party. The lifetime of a **client-server session** is a subset of the **server session**.
A **uid** is an unsigned 64-bit number that is unique *for a specific Server Session*. **uid**'s are shared between the server and all of its clients. A **uid** need not be unique for different servers connected to the same client.

All packets on the **Control** channel are of the form:

+------------------------+------------------------+
|          bytes         |     member             |
|                        |                        |
+========================+========================+
|      1                 | commandPayloadType     |
+------------------------+------------------------+
|      1+                | command                |
+------------------------+------------------------+



The **Setup Command** is

.. mermaid::

    ---
    config:
        packet:
            bitsPerRow: 4
            rowHeight: 40
            bitWidth: 128
            showBits: 1
            paddingX: 4
            paddingY: 4
    ---
	packet-beta
	  0-0: "CommandPayloadType"
	  1-4: "Debug stream index"
	  5-8: "Debug network packets?"
	  9-12: "Required Latency Ms"
	  13-16: "Required Latency Ms"
	  17-20: "Idle Connection Timeout"
	  21-28: "Server Session ID"

+-----------------------+--------------------+----------------------------------------------------------+
|          bytes        |        type        |    description                                           |
|                       |                    |                                                          |
+=======================+====================+==========================================================+
|      1                | CommandPayloadType | Setup=2                                                  |
+-----------------------+--------------------+----------------------------------------------------------+
|     4                 |    uint32          | Debug stream index                                       |
+-----------------------+--------------------+----------------------------------------------------------+
|     4                 |    uint32          | Debug network packets?                                   |
+-----------------------+--------------------+----------------------------------------------------------+
|     4                 |    int32           | Required Latency Ms                                      |
+-----------------------+--------------------+----------------------------------------------------------+
|     4                 |    uint32          | Idle Connection Timeout                                  |
+-----------------------+--------------------+----------------------------------------------------------+
|     8                 |    uid             | Server Session ID                                        |
+-----------------------+--------------------+----------------------------------------------------------+
|     137               |    VideoConfig     | video stream configuratn                                 |
+-----------------------+--------------------+----------------------------------------------------------+
|     4                 |    float           | Draw Distance (metres)                                   |
+-----------------------+--------------------+----------------------------------------------------------+
|      1                |    AxesStandard    | Axes standard                                            |
+-----------------------+--------------------+----------------------------------------------------------+
|      1                |    uint8_t         | Audio input enabled 1 or 0                               |
+-----------------------+--------------------+----------------------------------------------------------+
|      1                |    uint8_t         | SSL enabled, 1 or 0                                      |
+-----------------------+--------------------+----------------------------------------------------------+
|      8                |    int64           | starting timestamp, microseconds since Unix Epoch        |
+-----------------------+--------------------+----------------------------------------------------------+

- see teleport::core::SetupCommand - where

.. list-table:: VideoConfig
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 4
     - uint32_t
     - video_width
   * -  4
     - uint32_t
     - video_height
   * -  4
     - uint32_t
     - depth_width
   * -  4
     - uint32_t
     - depth_height
   * -  4
     - uint32_t
     - perspective_width
   * -  4
     - uint32_t
     - perspective_height
   * -  4
     - float   
     - perspective_fov
   * -  4
     - float   
     - nearClipPlane
   * -  4
     - uint32_t
     - webcam_width
   * -  4
     - uint32_t
     - webcam_height
   * -  4
     - int32_t
     - webcam_offset_x
   * -  4
     - int32_t
     - webcam_offset_y
   * -  4
     - uint32_t
     - use_10_bit_decoding
   * -  4
     - uint32_t
     - use_yuv_444_decoding
   * -  4
     - uint32_t
     - use_alpha_layer_decoding 
   * -  4
     - uint32_t
     - colour_cubemap_size
   * -  4
     - int32_t
     - compose_cube
   * -  4
     - int32_t
     - use_cubemap
   * -  4
     - int32_t
     - stream_webcam
   * -  4
     - avs::VideoCodec
     - videoCodec
   * -  4
     - int32_t
     - specular_x
   * -  4
     - int32_t
     - specular_y
   * -  4
     - int32_t
     - specular_cubemap_size
   * -  4
     - int32_t
     - specular_mips
   * -  4
     - int32_t
     - diffuse_x
   * -  4
     - int32_t
     - diffuse_y
   * -  4
     - int32_t
     - diffuse_cubemap_size
   * -  4
     - int32_t
     - light_x
   * -  4
     - int32_t
     - light_y
   * -  4
     - int32_t
     - light_cubemap_size
   * -  4
     - int32_t
     - shadowmap_x
   * -  4
     - int32_t
     - shadowmap_y
   * -  4
     - int32_t
     - shadowmap_size
   * -  4
     - float  
     - draw_distance

The Client sends a **Handshake** on the Handshake channel of the Data Service.

The **Handshake** is

+-----------------------+-------------------+-------------------------+
|          bytes        |        type       |    description          |
|                       |                   |                         |
+=======================+===================+=========================+
|     4                 | unsigned          | DisplayInfo width       |
+-----------------------+-------------------+-------------------------+
|     4                 | unsigned          | DisplayInfo height      |
+-----------------------+-------------------+-------------------------+
|     4                 | float             | metres per unit         |
+-----------------------+-------------------+-------------------------+
|     4                 | float             | field of view (degrees) |
+-----------------------+-------------------+-------------------------+
|     4                 | unsigned          | udp buffer size         |
+-----------------------+-------------------+-------------------------+
|     4                 | unsigned          | max bandwidth, kb/s     |
+-----------------------+-------------------+-------------------------+
|     1                 | AxesStandard      | Axes standard           |
+-----------------------+-------------------+-------------------------+
|     1                 | uint8_t           | framerate, Hz           |
+-----------------------+-------------------+-------------------------+
|     1                 | bool              | using hands             |
+-----------------------+-------------------+-------------------------+
|     1                 | bool              | is VR                   |
+-----------------------+-------------------+-------------------------+
|     8                 | uint64_t          | resource count = N      |
+-----------------------+-------------------+-------------------------+
|     4                 | uint32_t          | max lights supported    |
+-----------------------+-------------------+-------------------------+
|     4                 | uint32_t          | client Streaming Port   |
+-----------------------+-------------------+-------------------------+
|     4                 | int32_t           | minimum Priority        |
+-----------------------+-------------------+-------------------------+
|     8 * N             | uid               |resources already present|
+-----------------------+-------------------+-------------------------+

When the **Server** receives the **Handshake**, it starts streaming data. The handshake is of variable size, as it contains a list of the resource uid's that it already has cached.

The server then sends the ``AcknowledgeHandshake`` command on the **Command** channel:

.. list-table:: AcknowledgeHandshake
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - AcknowledgeHandshake=3
   * - 8
     - uint64_t
     - visibleNodeCount=N
   * - 8*N
     - uid
     - node uid's to be streamed

See :cpp:struct:`AcknowledgeHandshakeCommand`.

Once the **Client** has received the ``AcknowledgeHandshake``, initialization is complete and the client enters the main continuous Update mode.

Update
^^^^^^

.. toctree::
	:glob:
	:maxdepth: 2

	service/server_to_client
	service/client_to_server
