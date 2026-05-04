.. _geometry_payload:

################
Geometry Payload
################

The geometry channel carries every server-allocated resource that is not video, video-tag or audio. It is the **reliable / ordered** SCTP data channel labelled ``"geometry"`` (stream index 80). All multi-byte fields are little-endian and structs are 1-byte packed.

Reference implementations:

* Encoder — :cpp:class:`teleport::server::GeometryEncoder` (``Teleport/TeleportServer/GeometryEncoder.cpp``).
* Decoder — :cpp:class:`teleport::clientrender::GeometryDecoder` (``Teleport/ClientRender/GeometryDecoder.cpp``).
* Payload-type enum — :cpp:enum:`avs::GeometryPayloadType` (``libavstream/common_exports.h``).

Channel framing
===============

Each SCTP message on the geometry channel contains a single **payload chunk**:

.. list-table:: Geometry chunk header
   :widths: 5 14 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 8
     - size_t
     - ``payloadSize`` -- length, in bytes, of everything that follows the size field (i.e. ``type`` + ``uid`` + body).
   * - 1
     - GeometryPayloadType
     - Chunk type (see table below).
   * - 8
     - avs::uid
     - Resource uid this chunk belongs to. ``RemoveNodes`` is the only type that omits this field.

The ``payloadSize`` is *not* a self-delimiting framing prefix at the SCTP level; SCTP already preserves message boundaries. It is informational and is used by the decoder for sanity checking. Implementations that want to bundle multiple chunks per SCTP message can do so by emitting them back-to-back: the decoder advances ``offset`` by ``8 + payloadSize`` after each chunk.

Payload types
=============

.. list-table:: ``GeometryPayloadType``
   :widths: 5 18 60
   :header-rows: 1

   * - Id
     - Name
     - Body
   * - 0
     - Invalid
     - Reserved.
   * - 1
     - Mesh
     - 1-byte ``MeshCompressionType`` (``0`` none, ``1`` Draco), followed by version/name/Draco buffer. See :cpp:func:`GeometryEncoder::encodeMesh`.
   * - 2
     - Material
     - Material name + PBR descriptor and texture-uid references. See :cpp:func:`GeometryEncoder::encodeMaterial`.
   * - 3
     - MaterialInstance
     - Currently unused on the wire; reserved.
   * - 4
     - Texture
     - ``uint16`` name length, name, 1-byte ``TextureCompression``, then the codec-specific payload bytes (e.g. raw KTX2/Basis container). See :cpp:func:`GeometryEncoder::encodeTexture`.
   * - 5
     - Animation
     - Skeletal animation: name, joint count, per-joint translation/rotation/scale keyframes.
   * - 6
     - Node
     - ``uint16`` name length, name, ``avs::Transform`` (already converted to the client's axes), ``stationary`` flag, holder client id, priority, parent uid, then the type-specific data (mesh, light, link, …).
   * - 7
     - Skeleton
     - Name + bone-uid array.
   * - 8
     - FontAtlas
     - Font texture uid + glyph maps. See :cpp:func:`GeometryEncoder::encodeFontAtlas`.
   * - 9
     - TextCanvas
     - Backed text-canvas description.
   * - 10
     - TexturePointer
     - ``uint16`` URL length + URL bytes. The body is the HTTP(S) URL of an out-of-band Texture; see :doc:`http`.
   * - 11
     - MeshPointer
     - ``uint16`` URL length + URL bytes. As above but for Meshes.
   * - 12
     - MaterialPointer
     - Reserved; not currently emitted.
   * - 13
     - RemoveNodes
     - ``size_t`` count followed by that many ``avs::uid`` values to delete from the client's scene. **Has no resource uid in the header.**

Resource lifecycle
==================

For every chunk *other* than ``RemoveNodes``, ``TexturePointer``, ``MeshPointer`` and ``MaterialPointer``, the server records the uid as "in flight" and waits for the client to confirm receipt by sending ``ReceivedResourcesMessage`` (id 4) on the reliable client-to-server channel. See :doc:`service/client_to_server`.

For pointer chunks, the server treats the resource as delivered as soon as the chunk leaves the encoder; the actual asset is then fetched out-of-band over HTTPS. The client is still expected to send ``ReceivedResourcesMessage`` once the HTTP body has been decoded.

If the client's decoder fails (e.g. corrupt data, missing dependency, decoder panic), the client emits ``ResourceLostMessage`` (id 5) to ask the server to re-send. The server should treat the uid as "not yet received" and re-encode it on the next streaming pass.

Axis conversion
===============

Every geometric value (positions, transforms, vectors and quaternions) is converted from the server's :cpp:enum:`avs::AxesStandard` to the client's standard inside the encoder, using ``avs::ConvertTransform``. The client therefore reads geometry data in **its own** axes, regardless of how the source scene is authored. The server's axes are reported in ``SetupCommand.axesStandard`` for diagnostic purposes only.

Coding conventions
==================

* Strings are ``uint16``-length-prefixed UTF-8, never null-terminated.
* Variable-length arrays are ``size_t``-length-prefixed.
* Quaternions are ``vec4_packed`` ``(x, y, z, w)``; positions are ``vec3_packed`` in metres.
* Compression flags (``MeshCompressionType``, ``TextureCompression``) are 1-byte enums whose layout is part of the wire format; new values must bump :ref:`protocol_version <conventions>`.
