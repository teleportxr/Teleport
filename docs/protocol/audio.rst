.. _audio:

#####
Audio
#####

Audio is carried as one or more **WebRTC media tracks** (RTP / SRTP) negotiated in the same SDP exchange that creates the data channels described in :doc:`data_transfer`. Each track carries Opus, with parameters published to the client in the :ref:`audio_config` block of ``SetupCommand``.

In a multi-client (room) session the server acts as a **Selective Forwarding Unit (SFU)**: each client's microphone arrives at the server on one inbound track, and the server forwards a subset of those tracks to every other client as separate outbound tracks. Mapping of tracks to source client uids and admission decisions are signalled over the ``reliable`` data channel.

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

Tracks are identified end-to-end by their SDP ``mid`` attribute. The mapping ``mid → sourceClientUid`` is delivered on the ``reliable`` channel via the :ref:`audio_source_mapping` command; clients MUST NOT rely on parsing ``a=msid`` or any other SDP attribute for source identification.

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

If ``codec == 0`` no audio media tracks are present in the SDP, no ``AudioSourceMapping`` or ``AudioParticipantStateChange`` commands will be sent, and any client microphone state is ignored.

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
     - Selection is performed by application code on the server. Clients treat the resulting :ref:`audio_source_mapping` updates as authoritative.

When the ``symmetric routing`` flag (``AudioConfig.flags`` bit 2) is set, the SFU guarantees that if A is in B's selected set then B is in A's selected set; this may cause the actual forwarded count to exceed ``maxInboundStreams`` by at most one per pair affected.

The SFU MUST NOT forward a participant's own microphone back to them (loopback suppression).

Selection is recomputed on a server-defined cadence and on every join/leave. To avoid UI thrash on a peer hovering at the selection boundary, the server SHOULD apply the ``evictionGraceMs`` hysteresis before removing a transceiver that has just dropped out of the selected set.

.. _audio_source_mapping:

``AudioSourceMapping`` command
==============================

Sent by the server to a client whenever the set of audio tracks delivered to that client changes (a peer joined, left, was admitted by the selection policy, or was evicted). Carried on the ``reliable`` channel as a standard :ref:`server-to-client command <command_packet>`.

.. list-table:: AudioSourceMapping
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``AudioSourceMapping`` (id assigned in :doc:`service/server_to_client`).
   * - 2
     - uint16
     - ``addedCount`` = A.
   * - 2
     - uint16
     - ``removedCount`` = R.
   * - variable
     - AddedEntry[A]
     - Each: ``uint8 midLength``, ``midLength`` UTF-8 bytes (SDP ``mid``), ``uint64 sourceClientUid``.
   * - variable
     - RemovedEntry[R]
     - Each: ``uint8 midLength``, ``midLength`` UTF-8 bytes.

A client MUST treat the mapping as cumulative state: an ``Added`` entry whose ``mid`` is already known replaces the existing ``sourceClientUid``; a ``Removed`` entry whose ``mid`` is unknown is ignored. If a mapping arrives for a ``mid`` whose transceiver is not yet known locally (renegotiation race), the client MUST buffer it and apply it when the transceiver appears.

The very first ``AudioSourceMapping`` of a session may be sent with ``A == 0`` and ``R == 0`` to mean "the audio subsystem is ready; no peers are currently selected".

.. _audio_participant_state:

``AudioParticipantStateChange`` command
=======================================

Sent by the server to inform a client of changes to the audio state of *other* participants whose presence is otherwise visible (i.e. they are nodes in the scene). This is distinct from :ref:`audio_source_mapping`, which describes the transport-level set; ``AudioParticipantStateChange`` describes intent and is used to render UI ("out of range", "muted", "left") without the user mis-attributing silence to a fault.

.. list-table:: AudioParticipantStateChange
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 1
     - CommandPayloadType
     - ``AudioParticipantStateChange`` (id assigned in :doc:`service/server_to_client`).
   * - 2
     - uint16
     - ``updateCount`` = N.
   * - 10 × N
     - Update[N]
     - Each: ``uint64 sourceClientUid``, ``uint8 state``, ``uint8 reason``.

``state``:

* ``0`` ``Streaming`` — audio is being forwarded to this listener.
* ``1`` ``Culled`` — known participant excluded by the selection policy.
* ``2`` ``Disabled`` — participant is in the room but their microphone is off / muted by application policy.
* ``3`` ``Left`` — participant has disconnected.

``reason`` is informational and may be ``0`` (none). Defined non-zero values: ``1`` ``ProximityOut``, ``2`` ``CapExceeded``, ``3`` ``PolicyEvicted``, ``4`` ``ServerMuted``, ``5`` ``SelfMuted``.

Join and leave
==============

When peer X joins a room that already contains peers Y\ :sub:`1`, …, Y\ :sub:`k`:

1. The server adds, on X's PeerConnection: one ``recvonly`` transceiver for X's microphone, plus up to ``maxInboundStreams`` ``sendonly`` transceivers carrying the SFU-selected subset of {Y\ :sub:`i`}.
2. For each Y\ :sub:`i` whose selection set now contains X, the server adds one ``sendonly`` transceiver on Y\ :sub:`i`'s PeerConnection and triggers renegotiation per :doc:`signaling`.
3. The server sends ``AudioSourceMapping`` to X (listing all admitted Y\ :sub:`i`) and to every Y\ :sub:`i` whose set changed.
4. The server sends ``AudioParticipantStateChange`` to surface the user-visible state changes.

When peer X leaves, the reverse: outbound transceivers carrying X are stopped on every affected peer, ``AudioSourceMapping`` carries the removed ``mid``\ s, and ``AudioParticipantStateChange`` carries ``Left`` for X.

Example
=======

A 3-peer room with ``codec=Opus``, ``maxInboundStreams=2``, ``selectionPolicy=Proximity``, symmetric routing on:

.. code-block:: text

    Peer A's PeerConnection:        Peer B's PeerConnection:        Peer C's PeerConnection:
      mid=0  recvonly  (A's mic)      mid=0  recvonly  (B's mic)      mid=0  recvonly  (C's mic)
      mid=1  sendonly  (← B)          mid=1  sendonly  (← A)          mid=1  sendonly  (← A)
      mid=2  sendonly  (← C)          mid=2  sendonly  (← C)          mid=2  sendonly  (← B)

    AudioSourceMapping to A: added {mid=1→B.uid, mid=2→C.uid}
    AudioSourceMapping to B: added {mid=1→A.uid, mid=2→C.uid}
    AudioSourceMapping to C: added {mid=1→A.uid, mid=2→B.uid}

Lifecycle
=========

Audio media tracks are negotiated as part of the initial SDP offer/answer described in :doc:`signaling`. They become active as soon as DTLS-SRTP completes for that bundle; there is no separate ``StartAudio`` command. ``ShutdownCommand`` and any transport-level close end all audio tracks.

Mid-session reconfiguration of codec, sample rate or channel count is **not** supported: changes to :ref:`audio_config` require a new ``SetupCommand`` (i.e. a fresh session). Changes to ``maxInboundStreams``, ``selectionPolicy``, ``proximityRadiusMetres`` and ``evictionGraceMs`` MAY be applied at runtime by issuing a fresh ``SetupCommand`` with the same ``session_id``; in this case clients MUST re-apply the new policy parameters without dropping cached state.

