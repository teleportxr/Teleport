.. _audio:

#####
Audio
#####

Audio is carried as one or more **WebRTC media tracks** (RTP / SRTP) negotiated in the same SDP exchange that creates the data channels described in :doc:`data_transfer`. Each track carries Opus, with parameters published to the client in the :ref:`audio_config` block of ``SetupCommand``.

In a multi-client (room) session the server acts as a **Selective Forwarding Unit (SFU)**: each client's microphone arrives at the server on one inbound track, and the server forwards a subset of those tracks to every other client as separate outbound tracks. Each outbound stream carries a server-assigned :ref:`audio stream index <audio_stream_index>` and is bound to a scene node by an :ref:`audio emitter component <audio_emitter_component>`, so clients perform their own spatialisation. Which sources each listener receives is decided by the SFU's :ref:`selection policy <audio_selection>`. Client uids are never exposed to other clients.

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

Each outbound track carries one :ref:`audio stream index <audio_stream_index>`; the server sets the track's SDP ``mid`` to the decimal index, and this is the only binding between the RTP transport and the index. Clients read it from the received track (e.g. ``RTCRtpTransceiver.mid``) and MUST NOT infer a source from m-line order, ``a=msid`` or SSRC. The index is bound to a scene node by an :ref:`audio emitter component <audio_emitter_component>` delivered on the geometry channel.

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

If ``codec == 0`` no audio media tracks are present in the SDP, no audio emitter components are streamed, and any client microphone state is ignored.

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
     - Selection is performed by application code on the server. Clients treat the resulting :ref:`audio emitter component <audio_emitter_component>` updates as authoritative.

When the ``symmetric routing`` flag (``AudioConfig.flags`` bit 2) is set, the SFU guarantees that if A is in B's selected set then B is in A's selected set; this may cause the actual forwarded count to exceed ``maxInboundStreams`` by at most one per pair affected.

The SFU MUST NOT forward a participant's own microphone back to them (loopback suppression).

Selection is recomputed on a server-defined cadence and on every join/leave. To avoid UI thrash on a peer hovering at the selection boundary, the server SHOULD apply the ``evictionGraceMs`` hysteresis before removing a transceiver that has just dropped out of the selected set.

.. _audio_stream_index:

Audio stream index
==========================

Every outbound audio stream is tagged with an **audio stream index**: a 32-bit integer assigned by the server and unique within a session.

* Indices come from a strictly increasing per-session counter. ``0`` is reserved to mean *no stream* (see :ref:`audio_emitter_component`).
* An index is **never reused** within a session, even after the stream it named has stopped, so a client can discard decode state for a stopped stream with no risk of a late packet being misattributed to a new source. A session cannot plausibly exhaust 2\ :sup:`32` − 1 streams; a server that otherwise would MUST start a new session rather than wrap.
* The server encodes the index in the track ``mid`` (its decimal form). Because ``mid`` is immutable for the life of an m-line, the SFU MUST NOT recycle an m-line for a different stream: a stopped stream's m-line is set ``inactive`` and retired, and every new stream uses a new m-line with a new index. This is what makes non-reuse structural rather than a convention.

**Invalidation.** A stream stops when its source leaves, is evicted by the selection policy, or is muted. The server signals this by updating the owning node's :ref:`audio emitter component <audio_emitter_component>`: it clears the index to ``0`` if the source is still present but silent, or removes the component (or the whole node) if the source is gone. On either, the client stops and discards playback for that index, which is never allocated again.

.. _audio_emitter_component:

Audio emitter component
===============================

Audio is bound to the scene by an **audio emitter**, an optional component of a :doc:`node <geometry_payload>`. A node may carry an emitter *in addition to* a mesh or any other data, so an avatar is one node with both a mesh and an emitter. Emitters are added, updated and removed with their node on the geometry channel; there is no separate audio-mapping command.

.. list-table:: AudioEmitter
   :widths: 5 14 40
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 4
     - uint32
     - ``audioStreamIndex``. The :ref:`stream <audio_stream_index>` this emitter plays, or ``0`` when the emitter is present but currently silent (see ``reason``).
   * - 1
     - uint8
     - ``flags``. Bit 0 ``spatialised``: when clear, the emitter plays at constant ``gain`` and ignores the node transform. Other bits reserved, MUST be zero.
   * - 1
     - uint8
     - ``reason``. Why a ``0`` index is silent, for UI: ``0`` none, ``1`` OutOfRange, ``2`` CapExceeded, ``3`` Muted.
   * - 4
     - float
     - ``gain``. Linear playback gain; ``1.0`` = unity.
   * - 4
     - float
     - ``minDistanceMetres``. Distance below which no attenuation is applied (spatialised emitters only).
   * - 4
     - float
     - ``maxDistanceMetres``. Distance beyond which the emitter is inaudible (spatialised emitters only).

**Client-side spatialisation.** For a spatialised emitter the client computes attenuation (and panning) from the owning node's world transform relative to the listener, rolling off between ``minDistanceMetres`` and ``maxDistanceMetres``. This layers on top of the SFU's coarse admission: the server chooses *whether* a listener receives a stream, the client chooses *how loud*, so a source fades smoothly instead of cutting hard at the selection boundary.

**Orphan / non-spatial audio.** A source that should not be positioned — an announcer, a music bed, or a source whose node the client cannot place (culled, beyond ``drawDistance``, or not yet arrived) — is played non-spatially: either the server clears the ``spatialised`` flag, or the client falls back to non-spatial playback when it lacks the owning node's transform. Every source is still a node; a disembodied source is simply a node with no mesh and ``spatialised`` clear.

**Silent-but-present.** With ``audioStreamIndex == 0`` the emitter names no stream while the node persists, and ``reason`` lets the client render "muted" or "out of range" on the avatar. This subsumes the former ``AudioParticipantStateChange`` command.

.. note::
   The ``AudioSourceMapping`` and ``AudioParticipantStateChange`` command ids are **reserved (deprecated)**. Servers implementing this revision MUST NOT send them; clients MAY ignore them if received.

Join and leave
==============

When peer X joins a room that already contains peers Y\ :sub:`1`, …, Y\ :sub:`k`:

1. The server adds, on X's PeerConnection: one ``recvonly`` transceiver for X's microphone, plus up to ``maxInboundStreams`` ``sendonly`` transceivers for the SFU-selected subset of {Y\ :sub:`i`}. Each ``sendonly`` transceiver is given a fresh :ref:`audio stream index <audio_stream_index>` as its ``mid``.
2. The server streams to X, on the geometry channel, the node for each admitted Y\ :sub:`i` (if not already present), carrying an :ref:`audio emitter component <audio_emitter_component>` whose ``audioStreamIndex`` names that Y\ :sub:`i`'s track.
3. For each Y\ :sub:`i` whose selection set now contains X, the server adds one ``sendonly`` transceiver on Y\ :sub:`i`'s PeerConnection (a new index) and updates X's node there with an emitter component; renegotiation proceeds per :doc:`signaling`.

When peer X leaves, the reverse: the outbound transceivers carrying X are stopped and their m-lines retired on every affected peer, and X's node — with its emitter — is removed via ``RemoveNodes``. The indices X used are never reallocated.

Example
=======

A 3-peer room with ``codec=Opus``, ``maxInboundStreams=2``, ``selectionPolicy=Proximity``, symmetric routing on. ``A_node``, ``B_node`` and ``C_node`` are the peers' avatar nodes:

.. code-block:: text

    Peer A's PeerConnection:            Peer B's PeerConnection:            Peer C's PeerConnection:
      mid=0   recvonly (A's mic)          mid=0   recvonly (B's mic)          mid=0   recvonly (C's mic)
      mid=17  sendonly (stream 17)        mid=19  sendonly (stream 19)        mid=21  sendonly (stream 21)
      mid=18  sendonly (stream 18)        mid=20  sendonly (stream 20)        mid=22  sendonly (stream 22)

    To A:  B_node.emitter.audioStreamIndex = 17    C_node.emitter.audioStreamIndex = 18
    To B:  A_node.emitter.audioStreamIndex = 19    C_node.emitter.audioStreamIndex = 20
    To C:  A_node.emitter.audioStreamIndex = 21    B_node.emitter.audioStreamIndex = 22

A receives audio on ``mid=17``; ``B_node``'s emitter names stream 17, so A plays it positioned at ``B_node``'s transform.

Lifecycle
=========

Audio media tracks are negotiated as part of the initial SDP offer/answer described in :doc:`signaling`. They become active as soon as DTLS-SRTP completes for that bundle; there is no separate ``StartAudio`` command. Audio emitter components are streamed, updated and removed with their nodes on the geometry channel, and an individual stream ends when its :ref:`index is invalidated <audio_stream_index>`. ``ShutdownCommand`` and any transport-level close end all audio tracks.

Mid-session reconfiguration of codec, sample rate or channel count is **not** supported: changes to :ref:`audio_config` require a new ``SetupCommand`` (i.e. a fresh session). Changes to ``maxInboundStreams``, ``selectionPolicy``, ``proximityRadiusMetres`` and ``evictionGraceMs`` MAY be applied at runtime by issuing a fresh ``SetupCommand`` with the same ``session_id``; in this case clients MUST re-apply the new policy parameters without dropping cached state.

