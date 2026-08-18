# GltfConverter - glTF Text/Binary Conversion Utility

A command-line tool for converting between glTF's binary container (`.glb`) and its
text/JSON container (`.gltf`), in both directions. `.vrm` and `.vrma` files are treated
as glTF binary - they are glTF-binary containers carrying extra top-level extensions
(`VRM` for VRM 0.x, `VRMC_vrm`/`VRMC_vrm_animation` for VRM 1.0) - so they convert the
same way, with all extension JSON, buffers, and embedded images preserved byte-for-byte.

It can also **split a collection**: given one file holding several objects, `--split-objects`
writes each root object of the scene out as its own `.glb`, at its own origin, with every
texture written alongside as an external file.

## Building

### Prerequisites
- CMake 3.16+
- C++20 compiler
- TeleportCore library (built as part of Teleport; provides `tiny_gltf.h` and `<nlohmann/json.hpp>`)

### Build Steps

From the Teleport root directory:

```bash
cmake -B build_pc_client -S . -DTELEPORT_SERVER=ON
cmake --build build_pc_client --target GltfConverter --config Release
```

The executable will be at: `build_pc_client/bin/GltfConverter` (or similar depending on build path)

## Usage

```bash
GltfConverter [options] <input> [output]
```

### Options

| Flag | Description |
|------|-------------|
| `-h, --help` | Show help message |
| `-o, --output <path>` | Output file (default: same stem, opposite container extension) |
| `-s, --split-objects <dir>` | Export each root object of the scene as its own `.glb` in `<dir>`, with external textures |
| `-p, --pretty` | Pretty-print JSON output (text output only) |
| `-x, --external-buffers` | Write buffers/images as external files instead of embedding them (text output only) |
| `-v, --verbose` | Verbose output |

The input/output container is detected from the file extension: `.gltf` is text,
`.glb`/`.vrm`/`.vrma` are binary. If no output path is given, one is generated from the
input's stem with the container flipped (binary input → `.gltf`, text input → `.glb`).
To round-trip a `.vrm`/`.vrma` back to its original extension, pass it explicitly:

```bash
GltfConverter avatar.vrm avatar.gltf     # inspect/edit as JSON
GltfConverter avatar.gltf avatar.vrm     # convert back to .vrm
```

## Examples

### Convert a VRM avatar to readable JSON

```bash
GltfConverter avatar.vrm avatar.gltf --pretty
```

### Convert a VRMA animation to JSON and back

```bash
GltfConverter Idle.vrma Idle.gltf
GltfConverter Idle.gltf Idle.vrma
```

### Plain glTF binary/text round trip

```bash
GltfConverter scene.glb scene.gltf
GltfConverter scene.gltf scene.glb
```

### Split out buffers/images as external files

```bash
GltfConverter avatar.vrm avatar.gltf --external-buffers
# writes avatar.gltf plus avatar.bin (and any external image files)
```

## Splitting a collection into individual objects

```bash
GltfConverter collection.glb --split-objects objects/
```

Each **root object** of the input's scene becomes its own `.glb` in `objects/`, named after
its root node, and every texture in the file is written to the same directory as an ordinary
image file that the objects reference by URI:

```
objects/Chair.glb          objects/collection_0.png
objects/Table.glb          objects/collection_baseColour.jpg
objects/Lamp.glb           ...
```

- **Root objects** are the top-level nodes of the default scene whose subtree contains a mesh,
  camera or light. Empty locator roots are skipped.
- **At its own origin** means the exported root node's transform (`translation`/`rotation`/
  `scale`, or `matrix`) is dropped, so the object's own local origin becomes the world origin.
  Descendant transforms and all geometry bytes are untouched.
- **Textures become external files.** Images embedded in the container (or inlined as base64
  data URIs) are written out once, with their compressed bytes copied verbatim - never decoded
  or re-encoded - and the file extension taken from the declared mime type, or sniffed from the
  container's magic bytes. Images that already reference an external file keep their URI as
  authored; the file is copied next to the outputs when writing to another directory, so the
  URI still resolves. Textures are shared by all the exported objects rather than duplicated.
- **Skinning follows the mesh.** If a skin references joints outside the object's own subtree -
  a shared armature, say - those nodes and their ancestors come along, as extra roots of the
  exported scene, and a warning is printed because they keep their original placement while the
  object's root moves to the origin.
- **Animations** are subset per object: channels targeting nodes the object does not contain
  are dropped, along with any animation left with no channels.
- The input file is never overwritten; an object whose name matches the input's takes a
  numbered suffix instead.

### What each object contains

Only the arrays that carry binary weight are subset and reindexed: `nodes`, `meshes`, `skins`,
`accessors`, `bufferViews`, the buffer itself, and `animations`. `materials`, `textures`,
`images`, `samplers`, `cameras` and `lights` are copied whole, at their **original indices**.

That asymmetry is deliberate. Index references into those arrays also live inside extension
JSON that tinygltf carries as opaque `Value` data - `KHR_materials_*` texture infos,
`KHR_texture_basisu`, `KHR_texture_transform`, `KHR_lights_punctual` and others - and
reindexing around them would silently point materials at the wrong textures. Keeping them costs
a little JSON per object and no binary payload at all, because the images are external.

Whole-document extensions that index the original scene's nodes (`VRM`, `VRMC_vrm`,
`VRMC_vrm_animation`, `VRMC_springBone`, `VRMC_node_constraint`) cannot survive a split and are
dropped, with a warning. Splitting a VRM avatar therefore yields plain glTF meshes, not avatars.

## Losslessness

- **Buffers** are always handled by tinygltf's own embed/external logic - no custom
  code is needed, and no re-encoding happens.
- **Images** are never decoded to pixels. Images referenced via a `bufferView` (the
  normal case for VRM/VRMA/GLB textures) keep pointing at their original bytes
  untouched. Images referenced via an external `uri` keep that URI untouched without
  ever reading the referenced file. Images inlined as a base64 data URI (rare, only
  seen in some hand-authored `.gltf` files) have their raw bytes preserved and
  re-emitted as an equivalent data URI - never re-encoded.
- **Root-level extensions** (`VRM`, `VRMC_vrm`, `VRMC_vrm_animation`, and anything else
  attached at the document root) are preserved byte-for-byte. tinygltf's generic
  extension model silently drops empty JSON arrays/objects when it re-serializes
  extensions (common in real VRM files - e.g. `blendShapeGroups[].binds`/
  `materialValues` are frequently empty for unused presets); this tool works around
  that by capturing the verbatim original `extensions` JSON at load time
  (`SetStoreOriginalJSONForExtrasAndExtensions`) and splicing it back into the written
  output afterwards, so the VRM/VRMA payload survives exactly as authored. Extensions
  attached to individual nodes/materials/etc. still go through tinygltf's normal
  (mostly, but not guaranteed byte-identical) extension serialization.
- **Buffer 0** is always re-embedded as the GLB binary chunk (not a base64 URI) when
  writing binary output, even if it arrived as a base64-embedded `.gltf` buffer -
  otherwise a `.gltf → .vrm` conversion would silently balloon by ~33% and lose the
  proper GLB chunk structure.
- **Splitting** re-slices the buffer but never rewrites its contents: each object's buffer is
  the concatenation of exactly the bufferViews it uses, 4-byte aligned, with the original bytes
  copied through unchanged. Externalised textures are likewise byte-identical to the bytes that
  were embedded.
- **Animation targets** are written back into the split output afterwards, because tinygltf's
  channel serializer omits `target.node` when the node index is 0 (`if (channel.target_node > 0)`
  in `tiny_gltf.h`, where every other index field correctly tests against -1). Index 0 is the
  common case after a split - it is the object's own root - and a channel with no target is inert.

## Error Handling

The tool will report errors for:
- Input file not found
- Unrecognised input/output extension (must be `.gltf`, `.glb`, `.vrm`, or `.vrma`)
- glTF parse errors (malformed JSON, missing required fields, bad GLB chunks)
- Write failures (permissions, disk space)
- `--split-objects` combined with an output file, or given a file with no root object that has
  a mesh, camera or light
- `--split-objects` on an asset using `EXT_meshopt_compression`, whose bufferViews carry their
  own buffer offsets and so cannot be re-sliced (decompress it first)

Exit codes:
- **0** = Success
- **1** = Error (see stderr for details)

## Integration with TeleportCore

The utility links only against `TeleportCore`, which publicly exposes the vendored
`tiny_gltf.h` (`thirdparty/draco/third_party/tinygltf/`) and `<nlohmann/json.hpp>`
include paths. It does not need `TeleportServer`, `ktx`, or `Vulkan`.

`tiny_gltf.h` is compiled directly into `GltfConverter.cpp` (the tool's only translation
unit) with its `tinygltf` namespace renamed to `teleport_tinygltf`, matching the
convention used by `ClientRender/MeshDecoder.cpp` - this avoids a duplicate-symbol clash
with the separately-compiled `tinygltf::` symbols baked into the `draco` library.

## Limitations

- **No schema validation**: the tool does not validate VRM/glTF extension content, only
  the base glTF container structure (via tinygltf's own parser).
- **No mesh/material editing**: this is a container-format converter and splitter, not an asset
  editor. Splitting never re-encodes geometry or textures, and never merges or simplifies them.
- **Splitting drops whole-document extensions** (`VRM`, `VRMC_*`), which index the original
  scene's nodes.
- **`EXT_meshopt_compression` cannot be split.** `KHR_draco_mesh_compression` can - its
  bufferView reference is remapped - but any other extension that hides a `bufferView`,
  `accessor`, `node` or `mesh` index inside its JSON will not be remapped, because tinygltf
  keeps extension payloads as opaque values. Extensions referencing materials, textures,
  images, samplers, cameras or lights are safe by construction: those arrays keep their
  original indices.
