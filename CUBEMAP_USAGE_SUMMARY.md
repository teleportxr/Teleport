# KTX2 Cubemap Rearrangement - Complete Guide

## Quick Answer
To transform KTX2 cubemap files (Engineering ↔ OpenGL axes):

```cpp
#include "TeleportServer/CubemapRearrangement.h"

// Load, transform, save - all on compressed KTX2 data
ktxTexture2* src = CubemapKTX2Transformer::LoadKTX2File("input.ktx2");
ktxTexture2* dst = CubemapKTX2Transformer::ConvertEngineeringToOpenGL(src);
CubemapKTX2Transformer::SaveKTX2File(dst, "output.ktx2");
CubemapKTX2Transformer::DestroyTexture(src);
CubemapKTX2Transformer::DestroyTexture(dst);
```

## Why Not storeTexture()?
`GeometryStore::storeTexture()` requires:
- **Input**: Uncompressed pixel data in `avs::Texture::images[]`
- **Process**: Compress to KTX2 (expensive operation)
- **Problem**: You already have compressed KTX2 files

Better approach: Work directly with KTX library API on compressed data.

## The Right Tool: CubemapKTX2Transformer

### Static Methods
| Method | Input | Output | Purpose |
|--------|-------|--------|---------|
| `LoadKTX2File()` | File path | `ktxTexture2*` | Load .ktx2 from disk |
| `LoadKTX2Memory()` | Bytes buffer | `ktxTexture2*` | Load from memory |
| `SaveKTX2File()` | `ktxTexture2*` | File path | Write .ktx2 to disk |
| `SaveKTX2Memory()` | `ktxTexture2*` | Bytes buffer | Serialize to memory |
| `ConvertEngineeringToOpenGL()` | Source cubemap | Transformed cubemap | Swap Y↔Z axes |
| `ConvertOpenGLToEngineering()` | Source cubemap | Transformed cubemap | Reverse swap |
| `TransformFaceOrder()` | Source + mapping | Transformed cubemap | Custom face order |

## Use Cases

### Case 1: Single File Transformation
```cpp
TransformCubemapFile("environment_eng.ktx2", "environment_gl.ktx2");
```

### Case 2: In-Memory Pipeline
```cpp
std::vector<uint8_t> inputBytes = LoadFileToMemory("input.ktx2");
std::vector<uint8_t> outputBytes;
TransformCubemapInMemory(inputBytes, outputBytes);
SaveBytesToFile("output.ktx2", outputBytes);
```

### Case 3: Custom Face Rearrangement
```cpp
const CubemapKTX2Transformer::Face order[6] = { ... };
ktxTexture2* transformed =
    CubemapKTX2Transformer::TransformFaceOrder(source, order);
```

## Face Index Reference

KTX library face ordering:
- **0** = +X (right)
- **1** = -X (left)
- **2** = +Y (top/up)
- **3** = -Y (bottom/down)
- **4** = +Z (front)
- **5** = -Z (back)

### Engineering → OpenGL Mapping
```
Dst[0] ← Src[0]    // +X → +X (no change)
Dst[1] ← Src[1]    // -X → -X (no change)
Dst[2] ← Src[4]    // +Y ← +Z
Dst[3] ← Src[5]    // -Y ← -Z
Dst[4] ← Src[2]    // +Z ← +Y
Dst[5] ← Src[3]    // -Z ← -Y
```

## Implementation Files

### CubemapRearrangement.h
Declaration of `CubemapKTX2Transformer` class with all transformation methods.

### CubemapRearrangementExample.cpp
Four working examples:
1. `TransformCubemapFile()` - File-to-file transformation
2. `CustomFaceRearrangement()` - Arbitrary face ordering
3. `TransformCubemapInMemory()` - In-memory pipeline
4. `TransformCubemapDirectory()` - Batch processing skeleton

## Key Advantages
✓ No decompression needed (keeps KTX2 compressed)
✓ Fast face rearrangement (block-level operations)
✓ Preserves all compression metadata
✓ Works with file or memory I/O
✓ Supports all mip levels automatically
✓ Type-safe with KTX library API

## Dependencies
- KTX library (already in Teleport build)
- Standard C++ (no external deps)
