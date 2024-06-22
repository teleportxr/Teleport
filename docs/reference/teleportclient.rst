The Teleport Client library
===========================

Introduction
------------

TeleportClient is a library that provides client functionality, in particular client-specific networking and object management.

Tab Context
-----------

A TabContext is created for each connection. A TabContext is like a tab in a web browser, it can connect to a server, thus initializing a session. A TabContext can only be permanently maintain a single connection, but in transitioning it can have two connections: the existing connection that has already been established, and the new one that should replace it.

.. mermaid::

	flowchart TD
		subgraph "TabContext 1"
			A[Connection A] --> B[Connection B]
		end
		subgraph "TabContext 2"
			C[Connection C]
		end
		subgraph "TabContext 3"
			D[Connection D] --> E[Connection E]
		end
	
.. doxygenclass:: teleport::client::TabContext
	:members:

Session Client
--------------

When a connection is made, a SessionClient is created to manage it. The SessionClient has a state that should progress as follows:

.. mermaid::

	flowchart TD
		UNCONNECTED -- "User initiates connection" --> OFFERING
		OFFERING  -- "User initiates connection" --> AWAITING_SETUP
		AWAITING_SETUP --> HANDSHAKING
		HANDSHAKING --> CONNECTED

The SessionClient class is:
	
.. doxygenclass:: teleport::client::SessionClient
	:members:
	
Classes
-------

.. doxygenstruct:: teleport::client::ClientServerState
	:members:

.. doxygenclass:: teleport::client::ClientPipeline
	:members:

.. doxygenclass:: teleport::client::Config
	:members:

.. doxygenclass:: teleport::client::DiscoveryService
	:members:

.. doxygenclass:: teleport::client::OpenXR
	:members:

.. doxygenstruct:: teleport::client::SignalingServer
	:members:




