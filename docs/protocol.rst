########################
The Teleport VR Protocol
########################

The Teleport VR Protocol is an application-layer network protocol for client-server immersive applications.
Essentially this means that applications can be used remotely without downloading or installing them.
The protocol is open, meaning that anyone can use it, either by writing a client program or by developing a server that uses the protocol.
Simul Software provides reference implementations of both client and server.
The protocol and software should be considered pre-alpha, suitable for testing, evaluation and experimentation.

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



.. toctree::
	:glob:
	:maxdepth: 4
	:caption: Contents:
	
	protocol/discovery
	protocol/service
	protocol/data_transfer
	protocol/video
	protocol/video_metadata