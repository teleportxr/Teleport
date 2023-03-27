Server
======

Introduction
------------

TeleportServer is a library that provides server functionality in conjunction with a real-time game or simulation engine.

Networking
----------


.. mermaid::

	flowchart LR
		A(Video Queue) -->|1| H(Network Sink)
		B(Tag Queue) -->|2| H
		C(Audio Queue) -->|3| H
		D(Geometry Queue) -->|4| H
		E(Command Queue) -->|5| H
		F(Geometry Encoder) --> D

Classes
-------

.. doxygenclass:: teleport::server::AudioEncodePipeline
	:members:

.. doxygenclass:: teleport::server::AudioEncoder
	:members:

.. doxygenstruct:: teleport::server::ClientNetworkContext
	:members:

.. doxygenclass:: teleport::server::ClientData
	:members:

.. doxygenclass:: teleport::server::ClientManager
	:members:

.. doxygenclass:: teleport::server::ClientMessaging
	:members:

.. doxygenclass:: teleport::server::DiscoveryService
	:members:

.. doxygenclass:: teleport::server::GeometryEncoder
	:members:

.. doxygenclass:: teleport::server::GeometryStore
	:members:

.. doxygenclass:: teleport::server::GeometryStreamingService
	:members:

.. doxygenclass:: teleport::server::HTTPService
	:members:

.. doxygenclass:: teleport::server::DefaultHTTPService
	:members:

.. doxygenclass:: teleport::server::NetworkPipeline
	:members:

.. doxygenclass:: teleport::server::ServerSettings
	:members:

.. doxygenclass:: teleport::server::SourceNetworkPipeline
	:members:

.. doxygenclass:: teleport::server::VideoEncodePipeline
	:members:
