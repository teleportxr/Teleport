########################
The Teleport VR Protocol
########################

The Teleport VR Protocol is an application-layer network protocol for client-server immersive applications.
Essentially this means that applications can be used remotely without downloading or installing them.
The protocol is open, meaning that anyone can use it, either by writing a client program or by developing a server that uses the protocol.
Simul Software provides reference implementations of both client and server.
The protocol and software should be considered pre-alpha, suitable for testing, evaluation and experimentation.

Structure
---------
The protocol operates four main connections between a client and server:

+------------------------+---------------------------------------------------------------------------------------+
| Service Connection:    | Two-way communication between client and server, uses Enet over UDP.                  |
+------------------------+---------------------------------------------------------------------------------------+
| Video Stream:          | A one-way stream of video from server to client, uses SRT and EFP over UDP.           |
+------------------------+---------------------------------------------------------------------------------------+
| Geometry Stream:       | A one-way stream of geometry data from server to client, uses SRT and EFP over UDP.   |
+------------------------+---------------------------------------------------------------------------------------+
| Static data connection | An http/s connection for data files.                                                  |
+------------------------+---------------------------------------------------------------------------------------+

In addition, a Discovery connection is used only on connection startup, for new clients to connect to a server.
The Service Connection controls the others and is the main line of information between the client and server.

Discovery
---------

A server usually remains in the *Discovery* state while running.

The **Discovery Socket** is opened, as a nonblocking broadcast socket. It is bound to the server's address with the **Server Discovery Port** (default 10600).

A **Client** that wants to join (the **Connecting Client**) periodically sends to the server a four-byte **Connection Request** packet containing an unsigned 32-bit integer, the **Client ID**.

The **Discovery Service** listens on the **Discovery Socket** for a **Connection Request** from a **Connecting Client**. When one is received, if the server is set to filter client addresses, the **Connecting Client**'s address is filtered, and the connection is discarded it does not match the filter.

Otherwise, the server sends the **Connecting Client** on the **Client Discovery Port** a 6-byte **Service Discovery Response** consisting of the **Client ID** (4 bytes) and the **Service Server Port** (2 bytes as an unsigned 16-bit integer). The **Server Service Port** must be different for each **Client**.

In between periodically sending its **Connection Request** packets, a **Connecting Client** listens on the **Client Discovery Port** for the **Service Discovery Response**. On receiving this, it destroys its **Discovery Socket** and starts its **Service Connection** bound to the **Server Service Port**.

Streaming
---------
Initialization
^^^^^^^^^^^^^^
After a **Connecting Client** is discovered and accepted, the Server creates the Service Connection, a nonblocking connection bound to the **Server Service Port**. The **Service Connection** uses enet ([http://enet.bespin.org/](http://enet.bespin.org/)), a thin UDP manager, to ensure packet reliability and in-order delivery. The connection has eight channels: *Handshake, Control, DisplayInfo, HeadPose, ResourceRequest, KeyframeRequest, ClientMessage* and *Origin*.

The **Connecting Client** on receiving its **Service Discovery Response** opens the enet connection on its **Client Service Socket** as nonblocking, bound to the **Client Service Port**. This connects to the **Server** on the **Server Service Port**.

When the Server receives the **Enet Connection Request**, it considers **Discovery** to be complete for this **Client**. The **Server** sends the **Setup Command** to the **Client** on the **Service Connection**'s **Control** channel.

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

+-----------------------+-------------------+---------------------------+
|          bytes        |        type       |    description            |
|                       |                   |                           |
+=======================+===================+===========================+
|      1                | CommandPayloadType|    Setup=2                |
+-----------------------+-------------------+---------------------------+
|      4                |    int32          |    server streaming port  |
+-----------------------+-------------------+---------------------------+
|      4                |    int32          |    server http port       |
+-----------------------+-------------------+---------------------------+
|     4                 |    uint32         |    debug_stream           |
+-----------------------+-------------------+---------------------------+
|     4                 |     uint32        |    do_checksums           |
+-----------------------+-------------------+---------------------------+
|     4                 |    uint32         |    debug_network_packets  |
+-----------------------+-------------------+---------------------------+
|     4                 |    int32          |    requiredLatencyMs      |
+-----------------------+-------------------+---------------------------+
|     4                 |    uint32         | idle_connection_timeout   |
+-----------------------+-------------------+---------------------------+
|     8                 |    uid            | server ID                 |
+-----------------------+-------------------+---------------------------+
|     1                 |    ControlModel   | control_model             |
+-----------------------+-------------------+---------------------------+
|     137               |    VideoConfig    | video stream configuration|
+-----------------------+-------------------+---------------------------+
|     12 (3 floats)     |                   | bodyOffsetFromHead        |
+-----------------------+-------------------+---------------------------+
|      1                |    AxesStandard   | axes standard             |
+-----------------------+-------------------+---------------------------+
|      1                |    uint8_t        |audio input enabled, 1 or 0|
+-----------------------+-------------------+---------------------------+
|      1                |    uint8_t        |SSL enabled, 1 or 0        |
+-----------------------+-------------------+---------------------------+
|      8                |    int64          | starting timestamp        |
+-----------------------+-------------------+---------------------------+

The  **Client** then initiates its streaming connection with the given **Server Streaming Port** from the ****Setup Command****. The Client sends a **Handshake** on the Handshake channel of the Service Connection.

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
|     8                 | uint64_t          | resource count          |
+-----------------------+-------------------+-------------------------+
|     4                 | uint32_t          | max lights supported    |
+-----------------------+-------------------+-------------------------+
|     4                 | uint32_t          | client Streaming Port   |
+-----------------------+-------------------+-------------------------+
|     4                 | int32_t           | minimum Priority        |
+-----------------------+-------------------+-------------------------+


When the **Server** receives the **Handshake**, it starts streaming proper. The **Streaming Connection** is created on the **Server Streaming Port**, which is by default (**Server Service Port** + 1). The Handshake is of variable size, as it contains a list of the resource uid's that it already has cached.