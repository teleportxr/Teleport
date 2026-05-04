.. _data_transfer:

#############
Data Transfer
#############

The Teleport Data Service is the real-time transport that carries the six logical streams listed in :doc:`service`. Two transports are supported by the reference code:

* **WebRTC** (default): used by the reference server (``avs::WebRtcNetworkSink``) and the reference client (``avs::WebRtcNetworkSource``). Recommended for all new implementations.
* **SRT + EFP** (optional/legacy): compiled in only when ``TELEPORT_SUPPORT_SRT`` is defined; described at the end of this page for backward compatibility.

Reliability requirements
^^^^^^^^^^^^^^^^^^^^^^^^

Whichever transport is used, the protocol assumes:

1. Lost packets on reliable streams are resent within a bounded time window.
2. The receiver can cope with out-of-order packets.
3. The receiver can detect corrupted or incomplete payloads.
4. The receiver can recover -- e.g. by requesting a video keyframe or marking a resource lost.

WebRTC transport (default)
^^^^^^^^^^^^^^^^^^^^^^^^^^

Negotiation
~~~~~~~~~~~

A WebRTC PeerConnection is established between client and server using the
SDP/ICE exchange described in :doc:`signaling`. Once the PeerConnection
has reached the ``connected`` state, six SCTP data channels are opened.

Each channel has a fixed *label* and a recommended numeric *id*. The
client's :cpp:class:`avs::WebRtcNetworkSource` and the server's
:cpp:class:`avs::WebRtcNetworkSink` match channels by **label**, so any
implementation must use these exact strings:

.. list-table:: WebRTC data channels
   :widths: 6 14 7 7 7 25
   :header-rows: 1

   * - Id
     - Label
     - Reliable
     - Ordered
     - Two-way
     - Notes
   * - 20
     - ``video``
     - No
     - No
     - No
     - Maximum chunk size 64 KiB; payloads parsed as AVC/HEVC Annex-B.
   * - 40
     - ``video_tags``
     - No
     - No
     - No
     - One framed payload per video frame.
   * - 60
     - ``audio_server_to_client``
     - No
     - No
     - Yes
     - Bidirectional. Server-to-client carries the server's audio output; client-to-server carries microphone capture if ``SetupCommand.audio_input_enabled`` is non-zero.
   * - 80
     - ``geometry``
     - Yes
     - Yes
     - No
     - Reliable, ordered. Payloads use the parser described in :ref:`geometry_payload`.
   * - 100
     - ``reliable``
     - Yes
     - Yes
     - Yes
     - Carries all server-to-client commands and all reliable client-to-server messages (e.g. ``Acknowledgement``, ``ReceivedResources``).
   * - 120
     - ``unreliable``
     - No
     - No
     - Yes
     - Per-frame poses, input states, latency pong.

Framing
~~~~~~~

WebRTC SCTP data channels deliver complete messages, so no
fragmentation header is needed at the protocol level. The server splits
a payload that exceeds ``chunkSize`` (64 KiB for most channels) into
multiple SCTP messages internally; the receiver re-assembles them
before passing the payload to the decoder for that channel. From the
application's point of view, each ``send`` corresponds to one logical
payload (a command, a message, a video frame fragment, a geometry
chunk, etc.).

Recovery
~~~~~~~~

* **Video**: on any frame loss, the client sends a
  ``KeyframeRequest`` :ref:`message <client_to_server>` over the
  unreliable channel; the server forces the encoder to emit an IDR
  frame. See :ref:`video`.
* **Geometry**: the server only retires a resource from the pending
  set when the client sends a ``ReceivedResources`` message. If the
  client cannot decode a payload it can send ``ResourceLost`` to force
  resending.
* **Reliable channels**: SCTP itself guarantees delivery; no
  protocol-level recovery is required.

.. _legacy_srt_efp:

Legacy transport: SRT + EFP
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::

   This transport is only built when ``TELEPORT_SUPPORT_SRT`` is set
   at compile time. New implementations should use WebRTC.

The Teleport Protocol can also be carried over the Secure Reliable
Transport (SRT) wrapped in the Elastic Frame Protocol (EFP). SRT
provides retransmission of lost UDP packets; EFP provides framing,
fragmentation and per-stream sub-addressing inside SRT.

For the Simul fork of SRT, see https://github.com/teleportxr/srt.
For more on SRT, see https://www.haivision.com/resources/streaming-video-definitions/srt/.
For EFP, see https://github.com/teleportxr/efp.

EFP groups packets into **SuperFrames**. The EFP library provides
sending and receiving of these via its own header on each packet, which
contains the SuperFrame id, fragment index, and a presentation
timestamp; Teleport reuses the timestamp field as a per-stream
**stream-payload-id**. A corrupted SuperFrame is still delivered (with
``mBroken`` set) so the application can request retransmission --
the same recovery mechanisms described above for the WebRTC transport
apply.

EFP frame types and SuperFrames
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EFP adds four different types of headers of varying size to each network packet. The types used depend on the total size of the payload and the **Maximum Transmission Unit** (**MTU**) size.
Each payload will either have one packet with a **ElasticFrameType1** header or multiple packets with **ElasticFrameType2** headers.
Both of these headers contain a uint64 member called hPTS (Presentation Timestamp). This is unused by EFP but used by Teleport to store the **stream-payload-id**, a unique identifier for a payload.
All **ElasticFrameType** headers contain a uint8 member named hStreamID. This uniquely identifies the stream the packet belongs to. 
This allows for payloads from different streams to be sent to a client in parallel. 
EFP header types 1, 2 and 3 are currently only used but types 0 and 4 may be used in future versions.
For a detailed breakdown and explanation about the different kinds of **ElasticFrameType** headers, see here: https://edgeware-my.sharepoint.com/:p:/g/personal/anders_cedronius_edgeware_tv/ERnSit7j6udBsZOqkQcMLrQBpKmnfdApG3lehRk4zE-qgQ?rtime=swkDU08M2kg.

An **ElasticFrameType1** is of the form:

+-----------------------+-------------------+---------------------------+
|          bytes        |        type       |    description            |
|                       |                   |                           |
+=======================+===================+===========================+
|      1                |    uint8          |    hFrameType=1           |
+-----------------------+-------------------+---------------------------+
|      1                |    uint8          |    hStreamID              |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hSuperFrameNo          |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hFragmentNo            |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hOfFragmentNo          |
+-----------------------+-------------------+---------------------------+

An **ElasticFrameType2** is of the form:

+-----------------------+-------------------+---------------------------+
|          bytes        |        type       |    description            |
|                       |                   |                           |
+=======================+===================+===========================+
|      1                |    uint8          |    hFrameType=2           |
+-----------------------+-------------------+---------------------------+
|      1                |    uint8          |    hStreamID              |
+-----------------------+-------------------+---------------------------+
|      1                |ElasticFrameContent|    hDataContent           |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hSizeOfData            |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hSuperFrameNo          |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hOfFragmentNo          |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hType1PacketSize       |
+-----------------------+-------------------+---------------------------+
|      8                |    uint64         |    hPts                   |
+-----------------------+-------------------+---------------------------+
|      4                |    uint32         |    hDtsPtsDiff            |
+-----------------------+-------------------+---------------------------+
|      4                |    uint32         |    hCode                  |
+-----------------------+-------------------+---------------------------+

An **ElasticFrameType3** is of the form:

+-----------------------+-------------------+---------------------------+
|          bytes        |        type       |    description            |
|                       |                   |                           |
+=======================+===================+===========================+
|      1                |    uint8          |    hFrameType=3           |
+-----------------------+-------------------+---------------------------+
|      1                |    uint8          |    hStreamID              |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hSuperFrameNo          |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hType1PacketSize       |
+-----------------------+-------------------+---------------------------+
|      2                |    uint16         |    hOfFragmentNo          |
+-----------------------+-------------------+---------------------------+


The hDataContent member of a **ElasticFrameType2** is a **ElasticFrameContent** enum that refers to the type of data in the stream. 
The **ElasticFrameContent** enum includes common codecs such as HEVC and H264 for video and and also allows for the content type to specified as private or custom data.
The **stream-data-type** is a **ElasticFrameContent** enum and is passed to the **EFP Sender** by the server. 

EFP **SuperFrames** are an EFP class that are only used on the client by the **ElasticFrameProtocolReceiver** class which will be referred to as the **EFP Recevier** . 
**SuperFrames** store all the data for a payload as well as some metadata. **SuperFrames** are created by the **EFP Recevier** when the first fragment or packet of
a payload is received and updated as new packets belonging to the same payload are received.
When all packets have been received, the **SuperFrame** is complete and passed to the **receiveCallback**.
The mBroken member flag of the **SuperFrame** is set to true by the **EFP Recevier** when the **SuperFrame** is corrupted.
The **SuperFrame** is corrupted if not all frame fragments or packets are received before the user specified timeout or if some data received is invalid.
A corrupted **SuperFrame** will still be passed to the **receiveCallback** to inform the client application of the corruption.
It is then up to the client application to recover from the corruption such as by requesting the same payload again.
The hSuperFrameNo field in the **ElasticFrameType** header is used by EFP to determine what **SuperFrame** a packet belongs to.
The **EFP Sender** on the server maintains this value.


A **SuperFrame** is of the form:

+-----------------------+-------------------+---------------------------------------------+
|          bytes        |        type       |                  description                |  
|                       |                   |                                             |
+=======================+===================+=============================================+
|      8                |    uint64         |   mFrameSize   - content size in bytes      |
+-----------------------+-------------------+---------------------------------------------+
|      1                |    uint8*         |   pFrameData   - pointer to content         |
+-----------------------+-------------------+---------------------------------------------+
|      1                |ElasticFrameContent|   mDataContent - format of content          |
+-----------------------+-------------------+---------------------------------------------+
|      1                |    bool           |   mBroken      - corrupted data flag        |
+-----------------------+-------------------+---------------------------------------------+
|      8                |    uint64         |   mPts         - presentation timestamp     |
+-----------------------+-------------------+---------------------------------------------+
|      4                |    uint64         |   mDts         - decode timestamp           |
+-----------------------+-------------------+---------------------------------------------+
|      4                |    uint32         |   mCode        - data format code           |
+-----------------------+-------------------+---------------------------------------------+
|      1                |    uint8          |   mStreamID    - stream id                  |
+-----------------------+-------------------+---------------------------------------------+
|      1                |    uint8          |   mSource      - index of EFP Sender        |
+-----------------------+-------------------+---------------------------------------------+
|      1                |    uint8          |   mFlags       - superFrame slags (Unusued) |
+-----------------------+-------------------+---------------------------------------------+


The network packet structure is of the form:

+-----------------------+
|    Network Packet     |  
|                       |
+=======================+
|         UDP           |  
+-----------------------+
|         SRT           |   
+-----------------------+
|         EFP           |
+-----------------------+
|       Content         |
+-----------------------+




Initialization of EFP on the Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On the start of a **client-server session**, the server initializes SRT and starts listening for messages on a user specified socket.
An instance of an **ElasticFrameProtocolSender** class is created and the **MTU** size is passed to the constructor
This is 1450 for UDP which is the protocol SRT is built on.
One EFP sender is used for each client and only one sender is needed for all data streams.
After construction, the **sendCallback** member of the **ElasticFrameProtocolSender** instance is set to a user defined callback.
This callback is called in the sender's **packAndSendFromPtr** function for each EFP packet created.
The callback's job is to actually send the data to the client.
Any network transfer protocol can be used in the callback to send the data but Teleport exclusively uses SRT.
Each payload sent to the client will have a unique identifier (**stream-payload-id**) for each stream.
This is a uint64 value that is initialized to 0 on the start of the **client-server session** and incremented for each stream when a new payload is sent to the client.
The **stream-payload-id** value is written to **PTS** of each EFP packet header.

Sending Data from the Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On each frame or application update, the server will poll SRT to check the status of the connection for each client.
If the server and client are connected, the server will do the following to transfer data to the client for each stream:

1. The stream's data source is checked for any available data. 
2. The available data buffer, buffer size, **stream-data-type**, **stream-payload-id**, and **stream-id** are passed to the EFP sender's **packAndSendFromPtr** function.
3. The **stream-payload-id** is incremented. 
4. EFP **packAndSendFromPtr** function assembles the data into multiple network packets with **ElasticFrameType** headers as described previously.
5. The **sendCallback** is called for each packet.
6. The **sendCallback** calls the SRT API's **srt_sendmsg2** function, passing the remote socket identifier, the network packet and packet size.
7. This function will add the SRT and UDP headers to the network packet and send the packet to the client.


Initialization of EFP on the Client
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On receiving the **SetupCommand**, the client will initialize SRT and create and configure an SRT socket for receiving network packets from the server.
Subsequently, the client will create an instance of an **ElasticFrameProtocolReceiver** or **EFP Receiver** and this will remain in use until the end of the 
**server-client-session**. A **SuperFrame** timeout and **EFPReceiverMode** are passed to the **EFP Receiver** constructor.
The **SuperFrame** timeout specifies the length of time EFP will wait between receiving the first and last packet of a payload before marking the **SuperFrame** as broken.
This timeout could vary depending on the application. Teleport sets the timeout to 100ms. 
There are two types of **EFPReceiverMode**, **run-to-completion** and **threaded**.
When receiving a network packet in **run-to-completion** mode, EFP will update the **SuperFrame** and call the **receiveCallback** on the same thread that calls the receiver's **receiveFragmentFromPtr** function.
When receiving a network packet in **theaded** mode, EFP will update the **SuperFrame** and call the **receiveCallback** on a separate worker thread.
If the **receiveCallback** is low overhead and the target device has 4 or more cpu cores, then **run-to-completion** mode is the best option. Otherwise **threaded** mode is recommended.
The receiver's **receiveCallback** is then assigned to a function that accepts a **SuperFrame** in its signature. 
This function will receive a completed **SuperFrame** containing the server's payload for the application to process.



Receiving Data on the Client
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On each frame or application update, the client will use **srt_connect** to try make a connection to the server using the server ip and **server streaming port**
provided in the **Setup Command**. If a connection has been attempted, the client will poll SRT to check the status of the connection.
If the server and client are connected, the client will do the following to process incoming packets from the server:

1. On a separate thread, obtain network packets from the socket using SRT API **srt_recvmsg** function until there are no packets available. 
   A pointer to the buffer the packet is written to is passed to the function.
   The function returns 1 if there is a packet available, 0 if there's no packet available and -1 if the connection to the server has been lost.
   The **srt_recvmsg** function reverses what **srt_sendmsg2** does on the server, removing the UDP and SRT headers from the network packet.
2. Pass each packet to the **receiveFragmentFromPtr** function of the **EFP Receiver**.
   This function also requires a parameter for the **MTU** and the **EFP Receiver** index which can be set to 0.
3. EFP will process each packet to assemble a **SuperFrame** and pass each completed **SuperFrame** to the **readCallback**.
4. The callback creates a **Payload Info** structure from the mPTS, pFrameSize, pFrameData (payload) and mBroken members of the **SuperFrame**. 
5. The payload's stream is identified by the mStreamID member of the **SuperFrame** and the **Payload Info** structure is written to a designated thread-safe queue for the stream.
6. The main thread will read the **Payload Info** structure from the queue. If the value of mBroken is true, the application may request a new payload.
   If the data is not corrupted, the payload will be passed to the corresponding decoder for the stream.
   .


  


