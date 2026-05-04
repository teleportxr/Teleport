.. _signaling:

Signaling
=========

*Signaling* is the process whereby client and server exchange reliable messages for initial setup, as well as configuration changes and disconnection while the Teleport connection is active.

In the reference implementation, a single **WebSocket** connection per server is used for signaling. Any suitable bidirectional reliable transport may be used, but the message format below must be preserved.

Connection
----------

The server listens for WebSocket upgrade requests, typically on port 8080 (any port is acceptable as long as the client knows to try it). The reference client opens

.. code-block:: text

    ws://<host>:<port>/<path>

where ``<path>`` is taken from the ``teleport://`` URL the client was launched with. The path identifies the requested session on the server (it may be empty).

The full message exchange:

.. mermaid::

    sequenceDiagram
        participant C as Client
        participant S as Server
        C->>S: WebSocket open
        loop until response
            C->>S: connect (text)
        end
        S->>C: connect-response (text)
        S-->>C: offer (text)
        par
            S-->>C: candidate (text, one or more)
        and
            C-->>S: answer (text)
            C-->>S: candidate (text, one or more)
        end
        Note over C,S: WebRTC PeerConnection established
        S-->>C: setupCommand (over reliable data channel)
        C-->>S: Handshake (binary WebSocket frame)
        S-->>C: acknowledgeHandshake (over reliable data channel)
        Note over C,S: ...streaming...
        C->>S: disconnect (text)


Signal types
------------

All signaling messages are JSON objects that contain a ``teleport-signal-type`` member identifying the message type. Unknown ``teleport-signal-type`` values that are not handled at the signaling layer (i.e. anything other than ``connect`` and ``disconnect``) are forwarded to the WebRTC stack as part of the SDP/ICE exchange.

``connect``
^^^^^^^^^^^

Sent by the client. A **Client** that wants to join (the **Connecting Client**) periodically sends a ``connect`` message until it receives a ``connect-response``.

  .. code-block:: JSON

    {
        "teleport-signal-type":"connect",
        "content":
        {
            "clientID": 0,
            "teleport": "0.9",
            "identity": "<opaque identity string>"
        }
    }

``clientID`` is zero if the client has not yet connected to this server session, and may be a unique non-zero id if it is attempting to reconnect.
``teleport`` is the protocol version (currently ``"0.9"``).
``identity`` is an opaque string the client supplies for application-level authentication (may be empty).
``connect-response``
^^^^^^^^^^^^^^^^^^^^

Sent by the server in response to ``connect``.

  .. code-block:: JSON

    {
        "teleport-signal-type":"connect-response",
        "content":
        {
            "clientID": 397357935703467,
            "serverID": 13503465235793
        }
    }

``clientID`` and ``serverID`` are unsigned 64-bit numbers. They are unique on the server: no two clients can have the same ID. The ``serverID`` represents the session: if it matches a previous connection from the same client, cached resource and node ids persist; otherwise the client must clear all resource and node ids for this server.

``offer`` / ``answer``
^^^^^^^^^^^^^^^^^^^^^^

After ``connect-response``, the server initiates the WebRTC `ICE <https://en.wikipedia.org/wiki/Interactive_Connectivity_Establishment>`_ exchange. The server sends an ``offer``:

  .. code-block:: JSON

    {
        "teleport-signal-type": "offer",
        "sdp": "[sdp contents]"
    }

where ``[sdp contents]`` is a `Session Description Protocol (SDP) <https://en.wikipedia.org/wiki/Session_Description_Protocol>`_ string describing the six data channels (see :doc:`data_transfer`).

The client replies with an ``answer``:

  .. code-block:: JSON

    {
        "teleport-signal-type":"answer",
        "id":"1",
        "sdp":"[sdp contents]"
    }

``candidate``
^^^^^^^^^^^^^

Both sides also send one or more ICE candidates:

  .. code-block:: JSON

    {
        "teleport-signal-type":"candidate",
        "candidate":"[ICE candidate string]",
        "id":"1",
        "mid":"0",
        "mlineindex":0
    }

When each side has received the other's ``offer``/``answer`` and at least one viable ``candidate``, the WebRTC PeerConnection becomes ``connected`` and the data channels open.

Binary handshake
^^^^^^^^^^^^^^^^

Once the data channels are open the server sends the **Setup Command** (see :doc:`service`) on the reliable channel. The client replies with a :ref:`Handshake <handshake>` message **as a binary WebSocket frame on the signaling connection** (not on a data channel). This is the only binary message exchanged over the signaling WebSocket; all subsequent client-to-server traffic uses the data channels described in :doc:`data_transfer`.

``disconnect``
^^^^^^^^^^^^^^

Sent by the client to terminate the session cleanly:

  .. code-block:: JSON

    {
        "teleport-signal-type": "disconnect"
    }

On receipt the server tears down the WebRTC PeerConnection and removes per-client state. The signaling WebSocket may also be closed at the transport layer.

After connection
----------------

Once streaming is active, the signaling WebSocket is used for:

* further ICE ``candidate`` messages (network changes, NAT rebinding);
* renegotiation messages forwarded to the WebRTC stack;
* the client-to-server ``disconnect``.

All other application traffic (commands, geometry, video, audio, input, poses, acknowledgements) flows over the WebRTC data channels in :doc:`data_transfer`.
