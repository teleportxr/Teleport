.. _geometry_payload:

################
Geometry Payload
################

The geometry channel carries every server-allocated resource that is not video, video-tag or audio.
It is the **reliable / ordered** SCTP data channel labelled ``"geometry"`` (stream index 80) (if using EFP framing),
or ``"geometry_unframed"`` (if not using framing). All multi-byte fields are little-endian and structs are 1-byte packed.

Reference implementations (encoder ``teleport::server::GeometryEncoder`` in ``Teleport/TeleportServer/GeometryEncoder.cpp``; decoder ``teleport::clientrender::GeometryDecoder`` in ``Teleport/ClientRender/GeometryDecoder.cpp``; payload-type enum ``avs::GeometryPayloadType`` in ``libavstream/common_exports.h``).

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
     - 1-byte ``MeshCompressionType`` (``0`` none, ``1`` Draco), followed by version/name/Draco buffer. See :ref:`mesh_payload`.
   * - 2
     - Material
     - Material name + PBR descriptor and texture-uid references. See :ref:`material_payload`.
   * - 3
     - MaterialInstance
     - Currently unused on the wire; reserved. See :ref:`material_instance_payload`.
   * - 4
     - Texture
     - ``uint16`` name length, name, 4-byte ``TextureCompression``, then the codec-specific payload bytes (e.g. raw KTX2/Basis container). See :ref:`texture_payload`.
   * - 5
     - Animation
     - Skeletal animation: name, duration, per-bone position and rotation keyframe lists. See :ref:`animation_payload`.
   * - 6
     - Node
     - ``uint16`` name length, name, ``avs::Transform`` (already converted to the client's axes), ``stationary`` flag, holder client id, priority, parent uid, then the type-specific data (mesh, light, link, …). See :ref:`node_payload`.
   * - 7
     - Skeleton
     - Name + bone-uid array. See :ref:`skeleton_payload`.
   * - 8
     - FontAtlas
     - Font texture uid + glyph maps. See :ref:`font_atlas_payload`.
   * - 9
     - TextCanvas
     - Text-canvas description (font uid, layout, colour, text). See :ref:`text_canvas_payload`.
   * - 10
     - TexturePointer
     - ``uint16`` URL length + URL bytes. The body is the HTTP(S) URL of an out-of-band Texture; see :ref:`texture_pointer_payload` and :doc:`http`.
   * - 11
     - MeshPointer
     - ``uint16`` URL length + URL bytes. As above but for Meshes. See :ref:`mesh_pointer_payload`.
   * - 12
     - MaterialPointer
     - Reserved; not currently emitted. See :ref:`material_pointer_payload`.
   * - 13
     - RemoveNodes
     - ``uint16`` count followed by that many ``avs::uid`` values to delete from the client's scene. **Has no resource uid in the header.** See :ref:`remove_nodes_payload`.

.. _node_payload:

Node payload
============

A ``Node`` chunk describes a single scene-graph entry: its transform, parent, streaming metadata and an optional **component record** that attaches renderable data (mesh, light, text canvas) or a link. The chunk body that follows the standard geometry chunk header (``payloadSize``, ``GeometryPayloadType::Node``, ``uid``) is laid out as a fixed-size *node header* followed by zero or one component records.

(Reference: ``avs::Node``.)

Node header
-----------

.. list-table:: Node header
   :widths: 8 12 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - name length
     - ``uint16``
     - 2
     - UTF-8 byte count. The string is **not** null-terminated.
   * - name
     - ``uint8[name length]``
     - variable
     - Node name. Useful for diagnostics; the client does not key on it.
   * - localTransform
     - ``avs::Transform``
     - 40
     - ``vec3 position`` (12) + ``vec4 rotation`` (16) + ``vec3 scale`` (12). Already converted to the client's ``AxesStandard`` (see :ref:`conventions`) by the encoder.
   * - stationary
     - ``uint8``
     - 1
     - ``0`` = dynamic, non-zero = stationary. Stationary nodes are not expected to receive movement updates.
   * - holder_client_id
     - ``uint64`` (``avs::uid``)
     - 8
     - Owning client id, or ``0`` if the node is not held by any client.
   * - priority
     - ``int32``
     - 4
     - Streaming priority. The server may withhold the mesh of nodes below the client's minimum priority.
   * - parentID
     - ``uint64`` (``avs::uid``)
     - 8
     - Parent node uid, or ``0`` if the node is a scene-graph root.
   * - numComponents
     - ``uint8``
     - 1
     - Number of component records that follow. Currently ``0`` or ``1``: if ``0`` the node has no renderable data and the chunk ends here; if ``1`` exactly one component record follows. This is a count to leave room for additional components in future versions without bumping the header layout.

Once ``name`` has been consumed, the remainder of the header is a fixed 62-byte tail:

.. mermaid::

    packet-beta
        title Node header tail (byte offsets from end of name)
        0-39: "localTransform (avs::Transform)"
        40-40: "stationary"
        41-48: "holder_client_id"
        49-52: "priority"
        53-60: "parentID"
        61-61: "numComponents"

Component record
----------------

A component record is present only when ``numComponents != 0`` and begins with a single byte identifying the type:

.. list-table:: Component header
   :widths: 8 16 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size
     - Description
   * - data_type
     - ``uint8`` (``NodeDataType``)
     - 1
     - Identifies the component layout that follows; see ``NodeDataType`` table below.

``NodeDataType`` values (reference: ``avs::NodeDataType``):

.. list-table:: NodeDataType
   :widths: 5 22 60
   :header-rows: 1

   * - Id
     - Name
     - Body
   * - 0
     - Invalid
     - Reserved; not emitted.
   * - 1
     - None
     - Reserved; not emitted.
   * - 2
     - Mesh
     - Mesh component body (see below).
   * - 3
     - Light
     - Light component body (see below).
   * - 4
     - TextCanvas
     - TextCanvas component body (see below).
   * - 5
     - Unused1
     - Reserved; not emitted.
   * - 6
     - SkeletonUnused
     - Reserved; not emitted.
   * - 7
     - Link
     - Link component body (see below).
   * - 8
     - Script
     - Reserved; not emitted.

The body that follows the ``data_type`` byte depends on the type, and is one of the four layouts below.

Mesh component
^^^^^^^^^^^^^^

.. list-table:: Mesh component body
   :widths: 14 18 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - data_uid
     - ``uint64`` (``avs::uid``)
     - 8
     - Mesh resource uid. Must be non-zero; the mesh itself is sent as a separate ``Mesh`` or ``MeshPointer`` chunk.
   * - skeletonID
     - ``uint64`` (``avs::uid``)
     - 8
     - Skeleton resource uid, or ``0`` for an unskinned mesh.
   * - joint_indices count
     - ``uint16``
     - 2
     - Number of entries in the joint-indices array.
   * - joint_indices
     - ``int16[count]``
     - 2·*n*
     - Per-bone joint indices into the referenced skeleton. ``-1`` marks an unmapped bone.
   * - animations count
     - ``uint16``
     - 2
     - Number of entries in the animations array.
   * - animations
     - ``uint64[count]``
     - 8·*n*
     - Animation resource uids associated with this node.
   * - materials count
     - ``uint16``
     - 2
     - Number of entries in the materials array.
   * - materials
     - ``uint64[count]``
     - 8·*n*
     - Material resource uids; one per submesh.
   * - lightmapScaleOffset
     - ``vec4``
     - 16
     - Lightmap UV transform: ``xy`` scale, ``zw`` offset.
   * - globalIlluminationUid
     - ``uint64`` (``avs::uid``)
     - 8
     - Texture uid for the GI / lightmap texture, or ``0`` if the node is not lightmapped.

Light component
^^^^^^^^^^^^^^^

.. list-table:: Light component body
   :widths: 14 12 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - lightColour
     - ``vec4``
     - 16
     - Linear-space colour ``(r, g, b, a)``. Alpha is unused by the reference renderer.
   * - lightRadius
     - ``float``
     - 4
     - Light source radius, in metres.
   * - lightRange
     - ``float``
     - 4
     - Maximum effective distance of the light, in metres.
   * - lightDirection
     - ``vec3``
     - 12
     - Forward direction in the client's axes (already converted by the encoder).
   * - lightType
     - ``uint8``
     - 1
     - Light-type tag (point / spot / directional / area). See the ``lightType`` field on ``avs::Node``.

The Light component body is 37 bytes:

.. mermaid::

    packet-beta
        title Light component body (byte offsets after data_type)
        0-15: "lightColour"
        16-19: "lightRadius"
        20-23: "lightRange"
        24-35: "lightDirection"
        36-36: "lightType"

TextCanvas component
^^^^^^^^^^^^^^^^^^^^

.. list-table:: TextCanvas component body
   :widths: 14 18 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - data_uid
     - ``uint64`` (``avs::uid``)
     - 8
     - TextCanvas resource uid. The canvas contents are delivered separately as a ``TextCanvas`` chunk (id 9).

Link component
^^^^^^^^^^^^^^

.. list-table:: Link component body
   :widths: 14 18 12 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - url length
     - ``uint16``
     - 2
     - UTF-8 byte count of the target URL.
   * - url
     - ``uint8[url length]``
     - variable
     - Target URL activated when the user follows the link.
   * - query length
     - ``uint16``
     - 2
     - UTF-8 byte count of the query string.
   * - query
     - ``uint8[query length]``
     - variable
     - Optional query string appended to the URL on activation; empty for links that take no parameters.

Notes
-----

* String fields obey the file-wide convention from `Coding conventions`_: ``uint16``-length-prefixed UTF-8, never null-terminated.
* All vectors, quaternions and ``Transform`` values are already in the client's ``AxesStandard`` (see :ref:`conventions`) when they reach the wire.
* The ``numComponents`` byte is a forward-compatibility hook. Senders currently write ``0`` or ``1``; future revisions can attach multiple components to one node without a wire-level layout change.

.. _mesh_payload:

Mesh payload
============

A ``Mesh`` chunk carries a compressed mesh resource (geometry + skin binding). Only ``MeshCompressionType::DRACO`` is currently emitted; uncompressed meshes are not supported on the wire.

(In-memory representation: ``avs::CompressedMesh``.)

.. list-table:: Mesh body
   :widths: 18 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - meshCompressionType
     - ``uint8`` (``MeshCompressionType``)
     - 1
     - Compression scheme for the submesh buffers; see ``MeshCompressionType`` table below.
   * - version
     - ``uint16``
     - 2
     - Mesh-payload version. Currently ``1``.
   * - dracoVersion
     - ``int32``
     - 4
     - Draco bitstream version number. Currently ``1``.
   * - name length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - name
     - ``uint8[name length]``
     - variable
     - Mesh name.
   * - invBindDataSize
     - ``uint64``
     - 8
     - Length in bytes of the inverse-bind-matrix block that follows. ``0`` if the mesh is unskinned.
   * - invBindData
     - ``uint8[invBindDataSize]``
     - variable
     - Tightly packed array of ``mat4`` (16 floats = 64 bytes each), one per joint. Omitted when ``invBindDataSize == 0``.
   * - submeshCount
     - ``uint32``
     - 4
     - Number of submesh records that follow.
   * - submeshes
     - *see below*
     - variable
     - ``submeshCount`` records.

Each submesh record is:

.. list-table:: Submesh record
   :widths: 14 18 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - bufferSize
     - ``uint64``
     - 8
     - Length of the Draco buffer that follows.
   * - buffer
     - ``uint8[bufferSize]``
     - variable
     - Raw Draco-encoded submesh bytes; see the `Draco bitstream specification <https://google.github.io/draco/spec/>`_.

``MeshCompressionType`` values (reference: ``avs::MeshCompressionType``):

.. list-table:: MeshCompressionType
   :widths: 5 18 60
   :header-rows: 1

   * - Id
     - Name
     - Meaning
   * - 0
     - NONE
     - Uncompressed; not supported on the wire and rejected by the decoder.
   * - 1
     - DRACO
     - Each submesh buffer is a Draco-encoded bitstream.

.. _material_payload:

Material payload
================

A ``Material`` chunk carries a PBR (metallic-roughness) material descriptor and a list of extension records. Texture resources referenced by ``avs::uid`` are sent as separate ``Texture`` or ``TexturePointer`` chunks.

(In-memory representation: ``avs::Material``.)

.. list-table:: Material body
   :widths: 22 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - name length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - name
     - ``uint8[name length]``
     - variable
     - Material name.
   * - materialMode
     - ``uint8`` (``MaterialMode``)
     - 1
     - Blend / alpha behaviour; see ``MaterialMode`` table below.
   * - baseColorTexture
     - ``TextureAccessor``
     - 25
     - PBR base colour texture (see *TextureAccessor* below).
   * - baseColorFactor
     - ``vec4``
     - 16
     - Linear-space multiplier ``(r, g, b, a)``.
   * - metallicRoughnessTexture
     - ``TextureAccessor``
     - 25
     - Metallic + roughness texture.
   * - metallicFactor
     - ``float``
     - 4
     - Multiplier for the metallic channel.
   * - roughnessMultiplier
     - ``float``
     - 4
     - Multiplier for the roughness channel.
   * - roughnessOffset
     - ``float``
     - 4
     - Constant added to roughness after the multiplier.
   * - normalTexture
     - ``TextureAccessor``
     - 25
     - Normal map; the union member is interpreted as ``scale``. If the client did not declare ``normals`` support in the handshake, ``index`` is forced to ``0``.
   * - occlusionTexture
     - ``TextureAccessor``
     - 25
     - Ambient occlusion map; the union member is interpreted as ``strength``. If the client did not declare ``ambientOcclusion`` support, ``index`` is forced to ``0``.
   * - emissiveTexture
     - ``TextureAccessor``
     - 25
     - Emissive map.
   * - emissiveFactor
     - ``vec3``
     - 12
     - Linear-space emissive colour.
   * - doubleSided
     - ``uint8``
     - 1
     - Non-zero if the material should be rendered on both sides.
   * - lightmapTexCoordIndex
     - ``uint8``
     - 1
     - Index of the UV channel used for lightmaps.
   * - extensionCount
     - ``uint64``
     - 8
     - Number of material extensions that follow.
   * - extensions
     - *see below*
     - variable
     - ``extensionCount`` extension records.
   * - inlineTextureCount
     - ``uint64``
     - 8
     - Number of inline ``Texture`` chunks that follow this material on the wire. Currently always ``0``; texture resources are sent independently.

``TextureAccessor`` is a 25-byte fixed-size record:

.. list-table:: TextureAccessor
   :widths: 14 18 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - index
     - ``uint64`` (``avs::uid``)
     - 8
     - Texture resource uid, or ``0`` if absent.
   * - texCoord
     - ``uint8``
     - 1
     - UV channel index (``TEXCOORD_n``).
   * - tiling.x
     - ``float``
     - 4
     - UV tiling on the U axis.
   * - tiling.y
     - ``float``
     - 4
     - UV tiling on the V axis.
   * - scale / strength
     - ``float``
     - 4
     - Union: normal-map ``scale`` or occlusion ``strength``. Other slots ignore the value but the field is always present.

Each extension record begins with a 4-byte identifier:

.. list-table:: Extension record
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - id
     - ``uint32`` (``MaterialExtensionIdentifier``)
     - 4
     - Identifies the extension layout; see ``MaterialExtensionIdentifier`` table below.
   * - body
     - *id-specific*
     - variable
     - Layout defined by the named extension.

``MaterialMode`` values (reference: ``avs::MaterialMode``):

.. list-table:: MaterialMode
   :widths: 5 22 60
   :header-rows: 1

   * - Id
     - Name
     - Meaning
   * - 0
     - UNKNOWN
     - Unspecified; treated by the client as opaque.
   * - 1
     - OPAQUE
     - Fully opaque surface; alpha channel ignored.
   * - 2
     - TRANSPARENT
     - Alpha-blended surface.
   * - 3
     - MASKED
     - Alpha-tested cutout; fragments are kept or discarded based on the base-colour alpha threshold.

``MaterialExtensionIdentifier`` values (reference: ``avs::MaterialExtensionIdentifier``):

.. list-table:: MaterialExtensionIdentifier
   :widths: 5 22 60
   :header-rows: 1

   * - Id
     - Name
     - Meaning
   * - 0
     - SIMPLE_GRASS_WIND
     - Simple wind animation for grass-like vegetation.

.. _material_instance_payload:

MaterialInstance payload
========================

Reserved. ``MaterialInstance`` (id 3) is not currently emitted; the body is unspecified and receivers must skip the chunk by advancing past ``payloadSize`` bytes.

.. _texture_payload:

Texture payload
===============

A ``Texture`` chunk carries an inline compressed texture. Large textures (over ~1 MiB) are normally promoted to a :ref:`texture_pointer_payload` instead.

(In-memory representation: ``avs::Texture``.)

.. list-table:: Texture body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - name length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - name
     - ``uint8[name length]``
     - variable
     - Texture name (often the source file stem).
   * - compression
     - ``uint32`` (``TextureCompression``)
     - 4
     - Codec for ``compressedData``; see ``TextureCompression`` table below.
   * - compressedData
     - ``uint8[…]``
     - variable
     - Codec-specific payload. Its length is implied by ``payloadSize - (2 + nameLength + 4)``.

``TextureCompression`` values (reference: ``avs::TextureCompression``):

.. list-table:: TextureCompression
   :widths: 5 22 60
   :header-rows: 1

   * - Id
     - Name
     - Meaning
   * - 0
     - UNCOMPRESSED
     - Reserved; rejected on the wire.
   * - 1
     - PNG
     - Single PNG image.
   * - 2
     - MULTIPLE_PNG
     - Face/mip array of length-prefixed PNG blobs; the byte-exact layout is defined by the texture-cache writer in the reference server.
   * - 3
     - KTX
     - KTX2 / Basis container.
   * - 4
     - JPEG
     - Single JPEG image.

.. _animation_payload:

Animation payload
=================

An ``Animation`` chunk carries a per-bone keyframe track.

(In-memory representation: ``teleport::core::Animation``.)

.. list-table:: Animation body
   :widths: 18 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - name length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - name
     - ``uint8[name length]``
     - variable
     - Animation name.
   * - duration
     - ``float``
     - 4
     - Animation length in seconds. Must be ``>= 0``.
   * - boneTrackCount
     - ``uint64``
     - 8
     - Number of bone tracks that follow.
   * - tracks
     - *see below*
     - variable
     - ``boneTrackCount`` bone tracks.

Each bone track is:

.. list-table:: Bone track
   :widths: 18 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - boneIndex
     - ``int16``
     - 2
     - Index of the bone within the referencing skeleton; ``-1`` means unmapped.
   * - positionKeyframeCount
     - ``uint16``
     - 2
     - Number of position keyframes.
   * - positionKeyframes
     - ``{float time, vec3 value}[count]``
     - 16·n
     - Position keyframes in the client's axes. Time in seconds; value in metres.
   * - rotationKeyframeCount
     - ``uint16``
     - 2
     - Number of rotation keyframes.
   * - rotationKeyframes
     - ``{float time, vec4 value}[count]``
     - 20·n
     - Rotation keyframes. ``vec4`` is a quaternion ``(x, y, z, w)``.

.. _skeleton_payload:

Skeleton payload
================

A ``Skeleton`` chunk gives the ordered list of bone-node uids that make up a skeletal hierarchy. The bones themselves are normal scene nodes sent as :ref:`node_payload` chunks.

(In-memory representation: ``avs::Skeleton``.)

.. list-table:: Skeleton body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - name length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - name
     - ``uint8[name length]``
     - variable
     - Skeleton name.
   * - boneCount
     - ``uint64``
     - 8
     - Number of bone uids that follow.
   * - boneIDs
     - ``uint64[boneCount]``
     - 8·n
     - Bone-node uids, in the order expected by the Mesh component's ``joint_indices``.

.. _font_atlas_payload:

FontAtlas payload
=================

A ``FontAtlas`` chunk carries the glyph-position metadata for a font, keyed by point size. The actual glyph bitmap is delivered as a separate ``Texture`` whose uid is given here.

(In-memory representation: ``teleport::core::FontAtlas``.)

.. list-table:: FontAtlas body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - font_texture_uid
     - ``uint64`` (``avs::uid``)
     - 8
     - Texture uid of the font bitmap.
   * - mapCount
     - ``uint8``
     - 1
     - Number of font-size maps that follow. Maximum ``255``.
   * - maps
     - *see below*
     - variable
     - ``mapCount`` font maps.

Each font map is:

.. list-table:: Font map
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - pointSize
     - ``uint16``
     - 2
     - Font size key for this map.
   * - lineHeight
     - ``float``
     - 4
     - Line height in pixels for this size.
   * - glyphCount
     - ``uint16``
     - 2
     - Number of glyphs that follow.
   * - glyphs
     - ``Glyph[glyphCount]``
     - 30·n
     - Packed glyph records (see below).

A ``Glyph`` is a 30-byte packed record:

.. mermaid::

    packet-beta
        title Glyph (byte offsets)
        0-1: "index"
        2-3: "x0"
        4-5: "y0"
        6-7: "x1"
        8-9: "y1"
        10-13: "xOffset"
        14-17: "yOffset"
        18-21: "xAdvance"
        22-25: "xOffset2"
        26-29: "yOffset2"

* ``index, x0, y0, x1, y1`` are ``uint16`` and describe the bounding box of the glyph in the font texture.
* ``xOffset, yOffset, xAdvance, xOffset2, yOffset2`` are ``float`` and describe placement and advance, in the canvas's local units.

.. _text_canvas_payload:

TextCanvas payload
==================

A ``TextCanvas`` chunk carries the renderable contents of a text canvas referenced by a Node's :ref:`TextCanvas component <node_payload>`.

(In-memory representation: ``teleport::core::TextCanvas``.)

.. list-table:: TextCanvas body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - font_uid
     - ``uint64`` (``avs::uid``)
     - 8
     - FontAtlas uid this canvas renders with.
   * - pointSize
     - ``int32``
     - 4
     - Selected point size; must match one of the FontAtlas's ``pointSize`` keys.
   * - lineHeight
     - ``float``
     - 4
     - Per-canvas line-height override, in the canvas's local units.
   * - colour
     - ``vec4``
     - 16
     - Foreground colour ``(r, g, b, a)``.
   * - text length
     - ``uint16``
     - 2
     - UTF-8 byte count of the text payload.
   * - text
     - ``uint8[text length]``
     - variable
     - UTF-8 text. Not null-terminated.

The fixed 32-byte head:

.. mermaid::

    packet-beta
        title TextCanvas head (byte offsets)
        0-7: "font_uid"
        8-11: "pointSize"
        12-15: "lineHeight"
        16-31: "colour"

.. _texture_pointer_payload:

TexturePointer payload
======================

A ``TexturePointer`` chunk delivers a Texture indirectly: it carries an HTTP(S) URL that the client fetches and then decodes as a :ref:`texture_payload`. See :doc:`http` for the fetch and caching rules.

.. list-table:: TexturePointer body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - url length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - url
     - ``uint8[url length]``
     - variable
     - Absolute URL (``http://`` / ``https://``) or path relative to the cache's ``defaultURLRoot``.

Example body bytes for the URL ``"/a/b.ktx2"``::

    09 00                                    -- url length = 9
    2F 61 2F 62 2E 6B 74 78 32               -- "/a/b.ktx2"

.. _mesh_pointer_payload:

MeshPointer payload
===================

A ``MeshPointer`` chunk has the identical layout to :ref:`texture_pointer_payload`, but the body fetched from the URL is decoded as a :ref:`mesh_payload`.

.. list-table:: MeshPointer body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - url length
     - ``uint16``
     - 2
     - UTF-8 byte count.
   * - url
     - ``uint8[url length]``
     - variable
     - Absolute URL or relative path; see :doc:`http`.

.. _material_pointer_payload:

MaterialPointer payload
=======================

Reserved. ``MaterialPointer`` (id 12) is not currently emitted; receivers must skip the chunk by advancing past ``payloadSize`` bytes. The intended body matches :ref:`texture_pointer_payload` but for Material resources.

.. _remove_nodes_payload:

RemoveNodes payload
===================

The ``RemoveNodes`` chunk instructs the client to delete one or more scene nodes. It is the **only** payload type whose chunk header omits the ``uid`` field — there is no single resource this chunk is "about".

.. list-table:: RemoveNodes body
   :widths: 14 22 8 60
   :header-rows: 1

   * - Field
     - Type
     - Size (bytes)
     - Description
   * - count
     - ``uint16``
     - 2
     - Number of node uids that follow. Must be ``> 0``; senders skip emitting the chunk entirely when there is nothing to remove.
   * - nodeIDs
     - ``uint64[count]``
     - 8·n
     - Node uids to delete. Each delete is idempotent and silently succeeds if the node is unknown.

Example body removing two nodes with uids ``0x11`` and ``0x22``::

    02 00                                    -- count = 2
    11 00 00 00 00 00 00 00                  -- uid 0x11
    22 00 00 00 00 00 00 00                  -- uid 0x22

Resource lifecycle
==================

For every chunk *other* than ``RemoveNodes``, ``TexturePointer``, ``MeshPointer`` and ``MaterialPointer``, the server records the uid as "in flight" and waits for the client to confirm receipt by sending ``ReceivedResourcesMessage`` (id 4) on the reliable client-to-server channel. See :doc:`service/client_to_server`.

For pointer chunks, the server treats the resource as delivered as soon as the chunk leaves the encoder; the actual asset is then fetched out-of-band over HTTPS. The client is still expected to send ``ReceivedResourcesMessage`` once the HTTP body has been decoded.

If the client's decoder fails (e.g. corrupt data, missing dependency, decoder panic), the client emits ``ResourceLostMessage`` (id 5) to ask the server to re-send. The server should treat the uid as "not yet received" and re-encode it on the next streaming pass.

Axis conversion
===============

Every geometric value (positions, transforms, vectors and quaternions) is converted from the server's ``AxesStandard`` (see :ref:`conventions`) to the client's standard inside the encoder, using ``avs::ConvertTransform``. The client therefore reads geometry data in **its own** axes, regardless of how the source scene is authored. The server's axes are reported in ``SetupCommand.axesStandard`` for diagnostic purposes only.

Coding conventions
==================

* Strings are ``uint16``-length-prefixed UTF-8, never null-terminated.
* Variable-length arrays are ``size_t``-length-prefixed.
* Quaternions are ``vec4_packed`` ``(x, y, z, w)``; positions are ``vec3_packed`` in metres.
* Compression flags (``MeshCompressionType``, ``TextureCompression``) are 1-byte enums whose layout is part of the wire format; new values must bump :ref:`protocol_version <conventions>`.
