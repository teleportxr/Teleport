.. _audio:

#####
Audio
#####

Audio is carried as one or more **WebRTC media tracks** (RTP / SRTP) negotiated in the same SDP exchange that creates the data channels described in :doc:`data_transfer`. Each track carries Opus, with parameters published to the client in the :ref:`audio_config` block of ``SetupCommand``.

In a multi-client (room) session the server acts as a **Selective Forwarding Unit (SFU)**: each client's microphone arrives at the server on one inbound track, and the server forwards a subset of those tracks to every other client as separate outbound tracks. Each outbound track is :ref:`bound to a scene node <audio_node_binding>` by setting the track's SDP ``mid`` to the decimal uid of the node that emits it, so clients perform their own spatialisation. Which sources each listener receives is decided by the SFU's :ref:`selection policy <audio_selection>`. Client uids are never exposed to other clients.

Codec and RTP parameters
========================

Audio uses Opus (`RFC 6716 <https://www.rfc-editor.org/rfc/rfc6716>`_, payload format `RFC 7587 <https://www.rfc-editor.org/rfc/rfc7587>`_) with the following defaults, all of which may be changed via :ref:`audio_config`:

.. list-table::
   :widths: 25 10 40
   :header-rows: 1

   * - Parameter
     - Default
     - Notes
   * - RTP payload type
     - 111
     - Dynamic; advertised in the SDP ``a=rtpmap`` attribute.
   * - Sample rate
     - 48000 Hz
     - The Opus clock-rate. Decoder may resample for playback.
   * - Channels
     - 1
     - 2 (stereo) is permitted for music-grade sources.
   * - Frame duration
     - 20 ms
     - Permitted: 10, 20, 40, 60 ms.
   * - In-band FEC
     - on
     - Allows the decoder to reconstruct a lost packet from the next one.
   * - DTX
     - on
     - Discontinuous transmission during silence.

Implementations MUST advertise these in SDP (``a=fmtp:111 useinbandfec=1;usedtx=1;…``) and MUST honour the values received from the peer in the answered SDP.

Topology
========

For a session with N participants the server provisions transceivers per peer as follows. ``P`` denotes the per-listener cap :ref:`maxInboundStreams <audio_config>`.

.. list-table::
   :widths: 30 20 40
   :header-rows: 1

   * - Transceiver
     - Direction (server view)
     - Purpose
   * - 1 × per peer
     - ``recvonly``
     - The peer's microphone arriving at the server.
   * - ``min(P, N-1)`` × per peer
     - ``sendonly``
     - One outbound voice per other peer that the SFU has selected for this listener.

Each outbound track is :ref:`bound to a scene node <audio_node_binding>` by its SDP ``mid``, which the server sets to the decimal uid of the emitting node. This is the only binding between the RTP transport and the scene; clients read it from the received track (e.g. ``RTCRtpTransceiver.mid``) and MUST NOT infer a source from m-line order, ``a=msid`` or SSRC.

A client that does not provide microphone input still receives ``sendonly`` transceivers from the server (it is a *listener*); it may negotiate ``inactive`` on its own outbound m-line.

.. _audio_config:

``AudioConfig`` (inside ``SetupCommand``)
=========================================

A 17-byte block inside :ref:`setup_command` describing the audio configuration the server will use for this session. Clients MUST configure their decoder and microphone path to match.

.. list-table:: AudioConfig
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - uint8
     - ``codec``. ``0`` = audio disabled (no media tracks will be negotiated); ``1`` = Opus. Other values reserved.
   * - 1
     - uint8
     - ``rtpPayloadType`` (0–127). Must match the value in SDP.
   * - 4
     - uint32
     - ``sampleRateHz``. 48000 for Opus.
   * - 1
     - uint8
     - ``channelCount``. 1 or 2.
   * - 1
     - uint8
     - ``frameDurationMs``. 10, 20, 40 or 60.
   * - 1
     - uint8
     - ``flags``. Bit 0: in-band FEC. Bit 1: DTX. Bit 2: symmetric routing (see :ref:`audio_selection`). Other bits reserved, MUST be zero.
   * - 1
     - uint8
     - ``maxInboundStreams``. Per-listener cap. ``0`` means "no limit"; otherwise the SFU will forward at most this many concurrent voices to this client.
   * - 1
     - uint8
     - ``selectionPolicy``. ``0`` = ``All`` (no selection, requires ``maxInboundStreams == 0``), ``1`` = ``Fifo``, ``2`` = ``Proximity``, ``3`` = ``ActiveSpeaker``, ``4`` = ``Custom`` (server-side, opaque to client). See :ref:`audio_selection`.
   * - 4
     - float
     - ``proximityRadiusMetres``. Used only when ``selectionPolicy == Proximity``; informational for other policies.
   * - 2
     - uint16
     - ``evictionGraceMs``. Hysteresis applied by the SFU before evicting a peer that has fallen out of the selected set. ``0`` disables hysteresis.

If ``codec == 0`` no audio media tracks are present in the SDP and any client microphone state is ignored.

``SetupCommand.audio_input_enabled`` remains the gate on **client-to-server** microphone capture (the inbound transceiver on the server is set to ``inactive`` if it is zero).

.. _audio_selection:

Selection policy and caps
=========================

When the room has more potential speakers than ``maxInboundStreams``, the SFU chooses which sources each listener hears according to ``selectionPolicy``:

.. list-table::
   :widths: 18 60
   :header-rows: 1

   * - Policy
     - Rule
   * - ``All``
     - No selection: forward every other participant to every listener. Requires ``maxInboundStreams == 0``.
   * - ``Fifo``
     - Forward the first ``maxInboundStreams`` peers (by join order) to every listener.
   * - ``Proximity``
     - Forward the ``maxInboundStreams`` peers whose avatars are closest to the listener's avatar in world space, subject to ``proximityRadiusMetres``.
   * - ``ActiveSpeaker``
     - Forward the ``maxInboundStreams`` peers with the highest recent audio energy.
   * - ``Custom``
     - Selection is performed by application code on the server. Clients treat the set of forwarded tracks as authoritative.

When the ``symmetric routing`` flag (``AudioConfig.flags`` bit 2) is set, the SFU guarantees that if A is in B's selected set then B is in A's selected set; this may cause the actual forwarded count to exceed ``maxInboundStreams`` by at most one per pair affected.

The SFU MUST NOT forward a participant's own microphone back to them (loopback suppression).

Selection is recomputed on a server-defined cadence and on every join/leave. To avoid UI thrash on a peer hovering at the selection boundary, the server SHOULD apply the ``evictionGraceMs`` hysteresis before removing a transceiver that has just dropped out of the selected set.

.. _audio_node_binding:

Binding audio to nodes
======================

An outbound audio track is bound to the scene entirely by its SDP ``mid``: the server sets ``mid`` to the **decimal uid of the emitting node** — an avatar, or any object that emits sound. That node is an ordinary :doc:`node <geometry_payload>` on the geometry channel; nothing audio-specific is carried in the node payload, and there is no separate audio-mapping command.

* A track whose ``mid`` names a node is **spatialised** by the client at that node's world transform.
* ``mid`` ``0`` — or a ``mid`` naming a node the client cannot currently place (culled, beyond ``drawDistance``, or not yet arrived) — is played **non-spatially**. Non-spatial audio therefore needs no node: an announcer or music bed is simply a track with ``mid = 0``.

Because ``mid`` is immutable for the life of an m-line, the binding is stable for the whole session. When a source mutes/unmutes, or the SFU drops and later re-adds it, the server toggles that **same** m-line between ``sendonly`` and ``inactive`` rather than allocating a new one — so the node uid on the ``mid`` never changes, and a late packet arriving after ``inactive`` is unambiguously the same source. No per-stream index is needed.

**Client-side spatialisation.** For a track bound to a placed node the client computes attenuation (and panning) from that node's world transform relative to the listener. This layers on top of the SFU's coarse admission: the server chooses *whether* a listener receives a track, the client chooses *how loud*, so a source fades smoothly instead of cutting hard at the selection boundary. Gain and rolloff are client/application defaults and are **not** carried on the wire.

.. note::
   Earlier drafts carried an ``AudioEmitter`` node component and a per-stream *audio stream index*; both are withdrawn in favour of ``mid = node uid``. The ``NodeDataType`` value ``AudioEmitter`` and the ``AudioSourceMapping`` / ``AudioParticipantStateChange`` command ids remain **reserved** — servers MUST NOT send them, and clients MAY ignore them if received.

Join and leave
==============

When peer X joins a room that already contains peers Y\ :sub:`1`, …, Y\ :sub:`k`:

1. The server adds, on X's PeerConnection: one ``recvonly`` transceiver for X's microphone, plus up to ``maxInboundStreams`` ``sendonly`` transceivers for the SFU-selected subset of {Y\ :sub:`i`}. Each ``sendonly`` transceiver's ``mid`` is set to the uid of the Y\ :sub:`i` node it carries.
2. The server streams to X, on the geometry channel, the node for each admitted Y\ :sub:`i` (if not already present). No audio-specific payload is added; the binding is the track ``mid``.
3. For each Y\ :sub:`i` whose selection set now contains X, the server adds one ``sendonly`` transceiver on Y\ :sub:`i`'s PeerConnection with ``mid`` set to X's node uid; renegotiation proceeds per :doc:`signaling`.

When peer X leaves, the reverse: the outbound transceivers carrying X are stopped and their m-lines retired on every affected peer, and X's node is removed via ``RemoveNodes``.

Example
=======

A 3-peer room with ``codec=Opus``, ``maxInboundStreams=2``, ``selectionPolicy=Proximity``, symmetric routing on. The peers' avatar nodes have uids ``1001`` (A), ``1002`` (B) and ``1003`` (C). Each ``sendonly`` track's ``mid`` is the uid of the node it carries:

.. code-block:: text

    Peer A's PeerConnection:            Peer B's PeerConnection:            Peer C's PeerConnection:
      mid=0     recvonly (A's mic)        mid=0     recvonly (B's mic)        mid=0     recvonly (C's mic)
      mid=1002  sendonly (B's voice)      mid=1001  sendonly (A's voice)      mid=1001  sendonly (A's voice)
      mid=1003  sendonly (C's voice)      mid=1003  sendonly (C's voice)      mid=1002  sendonly (B's voice)

A receives B's voice on ``mid=1002`` — that is ``B_node``'s uid — so A plays it positioned at ``B_node``'s transform. (The listener's own mic m-line ``mid`` is arbitrary and never a node uid.)

Lifecycle
=========

Audio media tracks are negotiated as part of the initial SDP offer/answer described in :doc:`signaling`. They become active as soon as DTLS-SRTP completes for that bundle; there is no separate ``StartAudio`` command. The emitting nodes are streamed, updated and removed on the geometry channel like any other node; an individual voice ends when its m-line is set ``inactive`` or closed. ``ShutdownCommand`` and any transport-level close end all audio tracks.

Mid-session reconfiguration of codec, sample rate or channel count is **not** supported: changes to :ref:`audio_config` require a new ``SetupCommand`` (i.e. a fresh session). Changes to ``maxInboundStreams``, ``selectionPolicy``, ``proximityRadiusMetres`` and ``evictionGraceMs`` MAY be applied at runtime by issuing a fresh ``SetupCommand`` with the same ``session_id``; in this case clients MUST re-apply the new policy parameters without dropping cached state.

