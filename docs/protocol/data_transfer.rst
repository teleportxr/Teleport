#############
Data Transfer
#############


Reliability
^^^^^^^^^^^
To ensure the reliable streaming of content, the protocol requires four important features:

1. Lost network packets are resent within a certain time window.
2. The client can handle network packets received out of order.
3. The client can determine when a payload is corrupted.
4. The client/server can recover from a corrupted payload.

Secure Reliable Transfer Protocol
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The Secure Reliable Transfer (SRT) protocol is used for sending network packets. 
This is an open source protocol that provides a wrapper around UDP and allows for the retransmission of lost packets.
It has low overhead compared with TCP and deals with point 1 above. 
For more information on SRT, see here: https://www.haivision.com/resources/streaming-video-definitions/srt/

Elastic Frame Protocol
^^^^^^^^^^^^^^^^^^^^^^
The Teleport Protocol uses the Elastic Frame Protocol (EFP) to maximize streaming reliability. 
This is an open source protocol that is independent of the underlying transport protocol. 
It groups network packets together into large chunks of data called superframes. 
The EFP library provides functionality for sending and receiving superframes.  
EFP adds its own header to each network packet. 
The header contains information such as the superframe ID and the transmission timestamp. 
This information allows the EFP receiver to reassemble the superframe on the client and deal with points 2 and 3. 
When a payload is deemed to be corrupted, it is discarded and the client may request it to be sent again. 
How the client does this is dependant on the type of stream. See the documentation on each stream type for more information on this.
For EFP documentation, code examples and the source, see the Simul fork here: https://github.com/simul/efp.
Note: Teleport is currently using the master branch of the Simul fork of EFP and SRT.


EFP Frame Types and SuperFrames
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
EFP adds four different types of headers of varying size to each network packet. The types used depend on the total size of the payload and the **MTU** size.
Each payload will have either have one packet with a **ElasticFrameType1** header or multiple packets with **ElasticFrameType2** headers.
Both of these headers contain a uint64 member called hPTS (Presentation timestamp). This is unused by EFP but used by Teleport to store the **stream-payload-id**, a unique identifier for a payload.
All **ElasticFrameType** headers contain a uint8 member named hStreamID. This uniquely identifies the stream the packet belongs to. 
This allows for payloads from different streams to be sent to a client in parallel. 
EFP header types 1, 2 and 3 are currently only used but types 0 and 4 may be used in future versions.
For a detailed breakdown and explanation about the different kinds of **ElasticFrameType** headers, see here: https://edgeware-my.sharepoint.com/:p:/g/personal/anders_cedronius_edgeware_tv/ERnSit7j6udBsZOqkQcMLrQBpKmnfdApG3lehRk4zE-qgQ?rtime=swkDU08M2kg

**ElasticFrameType1**
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

**ElasticFrameType2**
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

**ElasticFrameType3**
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
**ElasticFrameContent** enum includes common codecs such as HEVC and H264 for video and a privatedata value to specify a custom content type.
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


**SuperFrame**
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


The network packet structure.
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



Sending Data from the Server
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Initialization
^^^^^^^^^^^^^^
On the start of a **client-server session**, the server initializes SRT and starts listening for messages on a user specified socket.
An instance of an **ElasticFrameProtocolSender** class is created and the Maximum Transmission Unit (MTU) is passed to the constructor
This is 1450 for UDP which is the protocol SRT is built on.
One EFP sender is used for each client and only one sender is needed for all data streams.
After construction, the **sendCallback** member of the **ElasticFrameProtocolSender** instance is set to a user defined callback.
This callback is called in the sender's **packAndSendFromPtr** function for each EFP packet created.
The callback's job is to actually send the data to the client.
Any network transfer protocol can be used in the callback to send the data but Teleport exclusively uses SRT.



Data Transfer Process
^^^^^^^^^^^^^^^^^^^^^
On each frame or application update, the server will poll SRT to check the status of the connection for each client.
If the server and client are connected, the server will do the following to transfer data to the client for each stream:

1. The stream's data source is checked for any available data. 
2. The available data buffer, buffer size, **stream-data-type**, **stream-payload-id**, and **stream-id** are passed to the EFP sender's **packAndSendFromPtr** function.
3. The **stream-payload-id** is increented.
4. EFP **packAndSendFromPtr** function assembles the data into multiple network packets with **ElasticFrameType** headers as described previously.
5. The **sendCallback** is called for each packet.
6. The **sendCallback** calls the SRT API's **srt_sendmsg2** function, passing the remote socket identifier, the network packet and packet size.
7. This function will add the SRT and UDP headers to the network packet and send the packet to the client.



Receiving Data on the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Initialization
^^^^^^^^^^^^^^
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



Data Receiving Process
^^^^^^^^^^^^^^^^^^^^^^
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
5. The payload's stream is identified by the mStreamID member of the **SuperFrame** and the **Payload Info** structure is written to a designated queue for the stream.
6. The main thread will read the **Payload Info** structure from the queue. If the value of mBroken is true, the application may request a new payload.
   If the data is not corrupted, the payload will be passed to the corresponding decoder for the stream.
   .


  


