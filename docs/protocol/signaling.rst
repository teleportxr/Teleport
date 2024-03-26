Signaling
=========

*Signaling* is the process whereby client and server exchange reliable messages for initial setup, as well as configuration changes while the Teleport connection is active.

In the reference implementation, WebSockets messages are used for signaling, but any suitable transport protocol may be used.

The server listens for connection requests, typically on the standard HTTP ports (any port is acceptable, as long as the client knows to try it).

A **Client** that wants to join (the **Connecting Client**) periodically sends to the server a **Connection Request** text packet. Unless the port is specified in the URL, a client will usually try a sequence of ports. The message is of the form:

  .. code-block:: JSON

	{
		"teleport-signal-type":"request",
		"content":
		{
			"clientID":0,
			"teleport":"0.9"
		},
	}

All signals have a member ``teleport-signal-type`` indicating their type. In the ``request`` message, ``clientID`` is zero if the client has not yet connected to this server session, and may be a unique nonzero id if it is attempting to reconnect.
Having received a ``request``, the server can respond with a **Request Response**, of the form:

  .. code-block:: JSON
	
	{
		"teleport-signal-type":"request-response",
		"content":
		{
			"clientID": 397357935703467
		}
	}

The ``clientID`` must be a positive unique 64-bit number.

The server will also initiate the process of creating a streaming connection. If the streaming protocol is WebRTC, this can be done using the `Interactive Connectivity Establishment <https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment>`_ protocol, which attempts to establish address/port combinations for client and server to communicate with minimal latency.

The server will send an ``offer`` of the form:

  .. code-block:: JSON
	
	{
	    "teleport-signal-type": "offer",
	    "sdp": "[sdp contents]"
	}
	
Where [sdp contents] is a text string in the `Session Description Protocol (SDP) <https://en.wikipedia.org/wiki/Session_Description_Protocol>`_ format. As well as the ``offer``, one or more ``candidate`` strings are sent:

.. code-block:: JSON

	{
		"teleport-signal-type":"candidate",
		"candidate":"[ICE candidate string]",
		"id":"1",
		"mid":"0",
		"mlineindex":0
	}

Where [ICE candidate string] contains an address, port and transport protocol that the client can use to stream data to the server.
Having received at least the ``offer``, the client responds with:

.. code-block:: JSON

	{
		"teleport-signal-type":"answer",
		"id":"1"
		,"sdp":"[sdp contents]"
	}

- and with its own ``candidate`` messages.

When each side of the connection has received the other's ``offer``/``answer`` and at least one ``candidate``, streaming can commence.
The signaling channel will be used only for changes of network configuration and disconnection messages, all others will use the **Data Service**.
