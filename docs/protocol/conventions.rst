.. _conventions:

###########
Conventions
###########

This page collects the cross-cutting conventions used by every wire format in the protocol. Implementers should read it before any of the per-channel pages.

Endianness, alignment and packing
=================================

* All multi-byte scalars on every channel are encoded **little-endian**, matching the layout used by the reference C++ implementation on the platforms it targets. (this is subject to change: network byte order may be preferable.)
* Every struct that crosses the wire is packed: implementations must use 1-byte alignment and assume **no implicit padding** between fields.
* ``bool`` is one byte. Id's are **8 bytes** (the protocol assumes a 64-bit platform on both ends).
* Strings sent on the geometry channel are UTF-8 and are **not** null-terminated; they are always preceded by an explicit ``uint16`` length.
* Implementations MUST bounds-check when reading binary data.
* Strings carried in JSON signaling messages follow normal JSON / UTF-8 rules.

Identifiers
===========

``uid``
    A 64-bit unsigned integer (``uint64``) used to identify every server-allocated resource (textures, meshes, materials, animations, skeletons, font atlases, text canvases, nodes, dynamic-lighting cubemaps, and the session itself). ``0`` is the reserved "invalid" uid. Uids are minted by the server and are unique within the lifetime of one server process; the client treats them as opaque.

``InputId``
    A 16-bit unsigned integer (``uint16_t``) that identifies one input declared by a :ref:`SetupInputsCommand <input>`. Input ids are unique within one ``SetupInputsCommand`` and become invalid when the next ``SetupInputsCommand`` is received.

``ack_id`` / ``confirmationNumber``
    Both are ``uint64``. ``ack_id`` is used by ``AckedCommand`` and is monotonically increasing per session — clients may discard any command whose ``ack_id`` is less than or equal to the highest one already processed. ``confirmationNumber`` is carried by ``NodeStateCommand`` and is acknowledged independently.

Coordinate systems and units
============================

Many commonly used renderers use different axis standards. While Y-vertical, right-handed is common in OpenGL applications, Unreal Engine uses Z-vertical, left-handed system. The protocol is intended to be easily implementable on differing hardware and software platforms, so it does not specify an axis standard but rather negotiates this between client and server.

Every server-to-client packet that contains geometric data is provided in the ``AxesStandard`` (reference: ``avs::AxesStandard``) that the client specified on connection. ``AxesStandard`` is a 1-byte bitfield with the following values on the wire:

.. list-table:: AxesStandard
   :widths: 12 18 50
   :header-rows: 1

   * - Value
     - Name
     - Meaning
   * - 0
     - ``NotInitialized``
     - Reserved; never sent on the wire.
   * - 1
     - ``RightHanded``
     - Handedness flag (component bit).
   * - 2
     - ``LeftHanded``
     - Handedness flag (component bit).
   * - 4
     - ``YVertical``
     - Vertical-axis flag (component bit).
   * - 8
     - ``ZVertical``
     - Vertical-axis flag (component bit).
   * - 10 (= 8 | 2)
     - ``EngineeringStyle``
     - Right-handed, +X right, +Y forward, +Z up (``ZVertical | RightHanded``).
   * - 21 (= 16 | 4 | 1)
     - ``GlStyle``
     - Right-handed, +X right, +Y up, -Z forward.
   * - 42 (= 32 | 8 | 2)
     - ``UnrealStyle``
     - Left-handed, Z up.
   * - 69 (= 64 | 4 | 1)
     - ``UnityStyle``
     - Left-handed, Y up.

A server must be capable of supporting clients in at least ``EngineeringStyle`` and ``GlStyle``.

* The client declares its native axes as part of the :ref:`Handshake <signaling>`.
* The server publishes the axes its scene is authored in via ``SetupCommand.axesStandard``. Clients should remember this value but it is informational only; numbers on the wire are always already in the client's standard.
* Linear units are **metres**.
* Quaternions are stored as ``vec4_packed`` with the layout ``(x, y, z, w)``.
* All transforms are local (relative to the parent node); root nodes are relative to the session origin specified by ``SetOriginNodeCommand``.

Time base
=========

Time is expressed in microseconds. The setup command specifies the session start time in UTC Unix time, and all other times on the wire are offsets from this.

.. list-table::
   :widths: 18 25 25
   :header-rows: 1

   * - Time base
     - Where it appears
     - Meaning
   * - Server UTC Unix microseconds (``int64``)
     - ``SetupCommand.startTimestamp_utc_unix_us``;
       ``PingForLatencyCommand.unix_time_us`` and the matching ``PongForLatencyMessage`` echo
     - The server's wall clock. Used as the session-start anchor and as the timestamp for round-trip / one-way latency estimation. The same clock in both fields, used for two different purposes.
   * - Client session-relative microseconds (``int64``)
     - 9-byte ``ClientMessage`` header (``timestamp_session_us``)
     - Microseconds elapsed on the client since its local session start. Monotonic within a session; not comparable across clients or to any UTC clock.

Versioning
==========

* ``Handshake.protocol_version`` and ``AcknowledgeHandshakeCommand`` together establish that the two endpoints understand the same wire format. The reference protocol version is **0.9**.
* There is currently no formal capability-negotiation handshake beyond ``RenderingFeatures`` and the WebRTC SDP exchange. Endpoints that disagree on ``protocol_version`` should drop the connection rather than try to negotiate.
* ``avs::VideoCodec``, ``avs::AudioCodec``, ``avs::AxesStandard`` and the ``CommandPayloadType`` / ``ClientMessagePayloadType`` / ``GeometryPayloadType`` enumerations are part of the wire format. Adding a new value is a backwards-incompatible change and must bump ``protocol_version``.
