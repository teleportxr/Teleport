.. _conventions:

###########
Conventions
###########

This page collects the cross-cutting conventions used by every wire format in the protocol. Implementers should read it before any of the per-channel pages.

Endianness, alignment and packing
=================================

* All multi-byte scalars on every channel are encoded **little-endian**, matching the layout used by the reference C++ implementation on the platforms it targets.
* Every struct that crosses the wire is declared with ``TELEPORT_PACKED`` (``__attribute__((packed, aligned(1)))`` on GCC/Clang, ``#pragma pack(push, 1)`` on MSVC). Implementations must use 1-byte alignment and assume **no implicit padding** between fields.
* ``bool`` is one byte. ``size_t`` is **8 bytes** (the protocol assumes a 64-bit platform on both ends).
* Strings sent on the geometry channel are UTF-8 and are **not** null-terminated; they are always preceded by an explicit ``uint16`` length.
* Strings carried in JSON signaling messages follow normal JSON / UTF-8 rules.

Identifiers
===========

``avs::uid``
    A 64-bit unsigned integer (``uint64_t``) used to identify every server-allocated resource (textures, meshes, materials, animations, skeletons, font atlases, text canvases, nodes, dynamic-lighting cubemaps, and the session itself). ``0`` is the reserved "invalid" uid. Uids are minted by the server and are unique within the lifetime of one server process; the client treats them as opaque.

``InputId``
    A 16-bit unsigned integer (``uint16_t``) that identifies one input declared by a :ref:`SetupInputsCommand <input>`. Input ids are unique within one ``SetupInputsCommand`` and become invalid when the next ``SetupInputsCommand`` is received.

``ack_id`` / ``confirmationNumber``
    Both are ``uint64_t``. ``ack_id`` is used by ``AckedCommand`` and is monotonically increasing per session — clients may discard any command whose ``ack_id`` is less than or equal to the highest one already processed. ``confirmationNumber`` is carried by ``NodeStateCommand`` and is acknowledged independently.

Coordinate systems and units
============================

The reference server is axis-agnostic: every server-to-client packet that contains geometric data is converted from the server's :cpp:enum:`avs::AxesStandard` to the client's at encode time.

* The client declares its native axes as part of the binary :ref:`Handshake <signaling>`.
* The server publishes the axes its scene is authored in via ``SetupCommand.axesStandard``. Clients should remember this value but it is informational only; numbers on the wire are always already in the client's standard.
* The reference value of "Engineering" is right-handed, +X right, +Y forward, +Z up; "GlStyle" is right-handed, +X right, +Y up, -Z forward.
* Linear units are **metres**.
* Quaternions are stored as ``vec4_packed`` with the layout ``(x, y, z, w)``.
* All transforms are local (relative to the parent node); root nodes are relative to the session origin selected by ``SetOriginNodeCommand``.

Time bases
==========

There are four distinct time bases on the wire. Implementations must keep them separate.

.. list-table::
   :widths: 18 14 25
   :header-rows: 1

   * - Time base
     - Where it appears
     - Meaning
   * - UTC Unix microseconds (``int64``)
     - ``SetupCommand.startTimestamp_utc_unix_us``
     - Wall-clock instant when the server session began.
   * - UTC Unix milliseconds (``int64``)
     - 9-byte ``ClientMessage`` header
     - Wall-clock instant the client created this message.
   * - UTC Unix nanoseconds (``int64``)
     - ``PingForLatencyCommand.unix_time_ns`` and the matching ``PongForLatencyMessage`` echo
     - Used only for round-trip / one-way latency estimation; never assume sub-microsecond accuracy.
   * - Stream ``uint32`` timestamp
     - First/last fragment of every ``NetworkPacket`` on the legacy SRT/EFP transport
     - Frame presentation time in the encoder's timebase. Not present on WebRTC SCTP messages.

Versioning
==========

* ``Handshake.protocol_version`` and ``AcknowledgeHandshakeCommand`` together establish that the two endpoints understand the same wire format. The reference protocol version is **0.9**.
* There is currently no formal capability-negotiation handshake beyond ``RenderingFeatures`` and the WebRTC SDP exchange. Endpoints that disagree on ``protocol_version`` should drop the connection rather than try to negotiate.
* ``avs::VideoCodec``, ``avs::AudioCodec``, ``avs::AxesStandard`` and the ``CommandPayloadType`` / ``ClientMessagePayloadType`` / ``GeometryPayloadType`` enumerations are part of the wire format. Adding a new value is a backwards-incompatible change and must bump ``protocol_version``.
