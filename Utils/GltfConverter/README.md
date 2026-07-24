# GltfConverter - glTF Text/Binary Conversion Utility

A command-line tool for converting between glTF's binary container (`.glb`) and its
text/JSON container (`.gltf`), in both directions. `.vrm` and `.vrma` files are treated
as glTF binary - they are glTF-binary containers carrying extra top-level extensions
(`VRM` for VRM 0.x, `VRMC_vrm`/`VRMC_vrm_animation` for VRM 1.0) - so they convert the
same way, with all extension JSON, buffers, and embedded images preserved byte-for-byte.

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

## Error Handling

The tool will report errors for:
- Input file not found
- Unrecognised input/output extension (must be `.gltf`, `.glb`, `.vrm`, or `.vrma`)
- glTF parse errors (malformed JSON, missing required fields, bad GLB chunks)
- Write failures (permissions, disk space)

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
- **No mesh/material editing**: this is a container-format converter, not an asset editor.
