.. _audio:

#####
Audio
#####

Audio is carried on a single bidirectional WebRTC SCTP data channel labelled ``"audio_server_to_client"`` (stream index 60). The same channel carries server output to the client and, when ``SetupCommand.audio_input_enabled`` is true, microphone input from the client back to the server.

Reference implementations:

* Server encoder pipeline — :cpp:class:`teleport::server::AudioEncodePipeline` (``Teleport/TeleportServer/AudioEncodePipeline.cpp``).
* Server input handler — :cpp:func:`teleport::server::ProcessAudioInput` (registered through ``Server_SetProcessAudioInputDelegate``).
* Wire enums — :cpp:enum:`avs::AudioCodec`, :cpp:enum:`avs::AudioPayloadType`.
* Header constant — :cpp:var:`avs::AudioParserInterface::HeaderSize` = 2.

Channel and framing
===================

* SCTP stream index: **60**, label ``audio_server_to_client``, reliable (set in ``ClientPipeline.cpp`` and ``NetworkPipeline.cpp``).
* Each SCTP message contains a single audio packet.
* Packet header is 2 bytes:

  .. list-table::
     :widths: 5 14 30
     :header-rows: 1

     * - Bytes
       - Type
       - Description
     * - 1
       - AudioPayloadType
       - Currently the only defined value is ``Capture`` (0).
     * - 1
       - reserved
       - Padding to align the payload to 2 bytes; ignored on receipt.

* Body: the codec-specific audio frame. For ``AudioCodec::PCM`` it is ``numChannels * (bitsPerSample/8) * samples`` interleaved bytes.

Codecs and parameters
=====================

The reference server defines the audio configuration in :cpp:struct:`teleport::server::AudioSettings`:

.. list-table::
   :widths: 18 14 35
   :header-rows: 1

   * - Field
     - Default
     - Notes
   * - ``codec``
     - ``AudioCodec::PCM``
     - The only codec value the reference encoder produces. ``Any`` / ``Invalid`` are accepted in negotiation but mean "unspecified".
   * - ``sampleRate``
     - 44100 Hz
     - Server-driven.
   * - ``bitsPerSample``
     - 16
     - Signed little-endian PCM.
   * - ``numChannels``
     - 2
     - Stereo (channels are interleaved L, R).

These parameters are **not** transmitted on the wire today: both endpoints must be configured to the same values out-of-band (the reference build hard-codes them on both sides). Future revisions are expected to publish them through a new ``AudioConfig`` block in ``SetupCommand`` analogous to ``VideoConfig``.

Server → client
===============

Each audio frame produced by the server's audio source is pushed through ``AudioEncodePipeline::sendAudio`` and emerges on the data channel as one ``AudioPayloadType::Capture`` packet. The reference server does not currently apply Opus or any other codec; every frame is raw interleaved PCM.

Client → server (microphone input)
==================================

When ``SetupCommand.audio_input_enabled`` is non-zero, the client may write microphone data on the same channel. Packet framing is identical: 2-byte header (``Capture``, padding) followed by the PCM body in the same sample format the server uses for output.

The server's ``CustomAudioStreamTarget`` invokes the registered ``processAudioInput(clientID, data, dataSize)`` delegate for every received packet. ``data`` and ``dataSize`` are the **payload only**, with the 2-byte header stripped.

If ``audio_input_enabled`` is zero, the client must not transmit on this channel; servers may close the connection on receipt of unsolicited audio.

Lifecycle
=========

Audio begins as soon as the channel reports open during the WebRTC negotiation; there is no separate ``StartAudio`` command. ``ShutdownCommand`` and any transport-level close end the audio stream.

Reconfiguration during a live session is not currently defined; servers that need to change sample rate or channel count must tear down the session and reissue ``SetupCommand``.

Quality and pacing
==================

* The reference server pushes audio frames at the rate produced by the source. There is no built-in jitter buffer beyond what the SCTP transport provides; clients should implement their own playout buffer (the reference client does this in ``CustomAudioStreamTarget``).
* Volume / gain is handled entirely on the consumer side; the wire format carries no per-packet metadata other than the type byte.

Operational notes for implementers
==================================

* If you implement a non-C++ client, treat unknown ``AudioPayloadType`` values as a hard error — the only known value is ``Capture``.
* The 2-byte header is the same for both directions. Do not omit the padding byte: ``HeaderSize == 2`` is a public constant in the reference parser.
* Because sample rate / bit depth / channel count are out-of-band today, an interoperable client must default to 44.1 kHz, 16-bit, stereo PCM unless it has private knowledge of an alternative configuration.
