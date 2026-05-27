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
        Note over C,S: WebRTC PeerConnection negotiating
        S-->>C: setupCommand (binary WebSocket frame OR reliable data channel)
        C-->>S: Handshake (binary WebSocket frame OR reliable data channel)
        S-->>C: acknowledgeHandshake (same transport as above)
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
            "identity": "<opaque identity string>",
            "capabilities": { "avatar_relay": true }
        }
    }

``clientID`` is zero if the client has not yet connected to this server session, and may be a unique non-zero id if it is attempting to reconnect.
``teleport`` is the protocol version (currently ``"0.9"``).
``identity`` is an opaque string the client supplies for application-level authentication (may be empty).
``capabilities`` is a free-form object advertising optional protocol features supported by the client. Unknown keys MUST be ignored. Currently defined keys: ``avatar_relay`` (boolean, see :ref:`signaling_avatars`).
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

.. _signaling_reliable_fallback:

Reliable-channel transport fallback (binary WebSocket frames)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The signaling WebSocket is **also** a transport for reliable-channel payloads. This is required because reliable-channel traffic — most importantly the ``SetupCommand`` and the client's binary ``Handshake`` reply — typically needs to be exchanged before the WebRTC PeerConnection has reached ``connected`` and the ``reliable`` data channel (label ``reliable``, id 100; see :doc:`data_transfer`) has reached ``open``.

Implementations therefore MUST support a dual reliable transport with the following rules.

Payload format
~~~~~~~~~~~~~~

* A reliable-channel payload sent on the signaling WebSocket is transmitted as a **single binary WebSocket frame** whose body is **byte-identical** to the payload that would be sent on the WebRTC ``reliable`` data channel. No additional framing, length prefix or envelope is added on either transport.
* Text frames on the signaling WebSocket carry only the JSON message types listed above; binary frames carry only reliable-channel payloads.

Sender rules (both directions)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* A sender MUST be able to deliver any reliable-channel payload over either transport.
* A sender MUST prefer the WebRTC ``reliable`` data channel once it has reached the ``open`` state, and MUST use the signaling WebSocket otherwise.
* The choice of transport is per payload: the sender MAY switch transports for the next payload as soon as the preferred one becomes available.
* A sender MUST NOT send the same logical payload over both transports.

Receiver rules (both directions)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* A receiver MUST accept reliable-channel payloads on both transports for the entire lifetime of the session, not only during early connection setup.
* A binary WebSocket frame and a message on the WebRTC ``reliable`` data channel MUST be dispatched through the **same** parser; the ``CommandPayloadType`` / ``ClientMessagePayloadType`` discriminator that begins every payload is sufficient to identify it.
* Ordering across the two transports is **not** guaranteed. Receivers MUST tolerate a reliable-channel payload arriving on the WebSocket after another payload sent later on the WebRTC ``reliable`` data channel (or vice-versa). Per-message ordering tools provided by the protocol (``ack_id``, ``confirmationNumber``) apply identically on both transports.

Examples
~~~~~~~~

The exchange around session start, with the WebRTC handshake still in flight when the server starts streaming setup:

.. mermaid::

    sequenceDiagram
        participant C as Client
        participant S as Server
        Note over C,S: signalling WebSocket open; WebRTC ICE in progress
        S-->>C: SetupCommand (binary WS frame)
        C-->>S: Handshake (binary WS frame)
        Note over C,S: WebRTC reliable channel reaches "open"
        S-->>C: AcknowledgeHandshake (reliable data channel)
        C-->>S: ReceivedResources (reliable data channel)

The exchange when the WebRTC handshake finished before the server sends setup:

.. mermaid::

    sequenceDiagram
        participant C as Client
        participant S as Server
        Note over C,S: WebRTC reliable channel already open
        S-->>C: SetupCommand (reliable data channel)
        C-->>S: Handshake (reliable data channel)
        S-->>C: AcknowledgeHandshake (reliable data channel)

.. _signaling_avatars:

Avatar negotiation
^^^^^^^^^^^^^^^^^^

If the server supports user avatars it may send an ``avatar-policy`` at any time after ``connect-response``. The client replies with an ``avatar-offer``; the server confirms with an ``avatar-result``. Servers may later send ``avatar-revoke`` to invalidate an accepted avatar. In **relay** mode the server forwards the owner's URL and proof to peers as ``peer-avatar`` messages, and a peer that fails to fetch the URL replies ``peer-avatar-failed``.

The full protocol — wire fields, requirements bag, proof schemes, security and privacy model — is specified in ``plans/avatars_plan.md`` in the source tree.

  .. code-block:: JSON

    { "teleport-signal-type": "avatar-policy",
      "content": { "policy_id": 12345, "requirement": "optional",
                   "default_available": true,
                   "requirements": { "formats": ["glb"], "max_file_bytes": 8388608 },
                   "proof": { "required": false, "accepted_schemes": [] } } }

  .. code-block:: JSON

    { "teleport-signal-type": "avatar-offer",
      "content": { "policy_id": 12345, "have_avatar": true,
                   "url": "https://avatars.example.com/u/42.glb",
                   "content_hash": "sha256:abcd",
                   "declared": { "format": "glb", "file_bytes": 4096 },
                   "allow_relay": true } }

  .. code-block:: JSON

    { "teleport-signal-type": "avatar-result",
      "content": { "policy_id": 12345, "status": "accepted",
                   "node_uid": 999, "using_default": false, "delivery": "relay",
                   "reasons": [] } }

Relay-mode peer messages:

  .. code-block:: JSON

    { "teleport-signal-type": "peer-avatar",
      "content": { "peer_client_id": 100, "peer_node_uid": 200,
                   "url": "https://avatars.example.com/u/42.glb",
                   "content_hash": "sha256:abcd", "format": "glb",
                   "revoked": false } }

  .. code-block:: JSON

    { "teleport-signal-type": "peer-avatar-failed",
      "content": { "peer_node_uid": 200, "reason": "fetch_timeout" } }

Avatar negotiation only runs when the client advertises support in ``connect.content.capabilities``. A client that omits the field (or sets ``avatar_relay`` to false) only receives **import**-mode avatars — i.e. ``avatar-result.delivery == "import"`` and avatars stream through the standard geometry pipeline rather than as ``peer-avatar`` messages.

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
* the client-to-server ``disconnect``;
* the reliable-channel transport fallback described in :ref:`signaling_reliable_fallback` — this remains available for the lifetime of the session, although in normal operation it falls silent once the WebRTC ``reliable`` data channel is open.

Geometry, video, per-frame input and pose traffic always flows over the WebRTC data channels in :doc:`data_transfer`, and audio always flows over the WebRTC media tracks in :doc:`audio`; only the ``reliable``-channel payloads have the dual-transport behaviour above.
