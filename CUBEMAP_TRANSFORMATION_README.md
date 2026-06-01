# KTX2 Cubemap Transformation Library

Utilities for loading, rearranging, and saving KTX2 cubemap files without decompression.

## Overview

When you need to transform cubemap orientation (e.g., swap Y/Z axes or convert between Engineering and OpenGL coordinate systems), this library provides direct access to KTX2 file manipulation via the KTX library API.

**Key feature**: Works entirely with compressed KTX2 data - no decompression to uncompressed images needed.

## Files

### 1. `CubemapRearrangement.h`
Header file declaring `CubemapKTX2Transformer` class with static methods:
- `LoadKTX2File(path)` - Load .ktx2 file
- `LoadKTX2Memory(data, size)` - Load from memory buffer
- `SaveKTX2File(texture, path)` - Write .ktx2 file
- `SaveKTX2Memory(texture, buffer)` - Serialize to memory
- `TransformFaceOrder(src, mapping)` - Custom face rearrangement
- `ConvertEngineeringToOpenGL(src)` - Swap Y/Z axes
- `ConvertOpenGLToEngineering(src)` - Reverse swap
- `IsCubemap(texture)` - Validate cubemap
- `DestroyTexture(texture)` - Free resources

### 2. `CubemapKTX2Transformer.cpp`
Implementation of all transformation functions using KTX library API.

Handles:
- Loading/saving from file and memory
- Creating new KTX2 texture objects with correct specifications
- Copying face data with user-defined mappings
- Pre-built Engineering ↔ OpenGL transformations

### 3. `CubemapRearrangementExample.cpp`
Four working examples demonstrating usage:
- `TransformCubemapFile()` - Simple file-to-file transformation
- `CustomFaceRearrangement()` - Arbitrary face reordering
- `TransformCubemapInMemory()` - In-memory pipeline
- `TransformCubemapDirectory()` - Batch processing skeleton

## Usage Pattern

```cpp
#include "CubemapRearrangement.h"

// Load
ktxTexture2* source = CubemapKTX2Transformer::LoadKTX2File("input.ktx2");

// Transform (choose one)
ktxTexture2* result = CubemapKTX2Transformer::ConvertEngineeringToOpenGL(source);
// OR
const CubemapKTX2Transformer::Face custom[6] = { ... };
ktxTexture2* result = CubemapKTX2Transformer::TransformFaceOrder(source, custom);

// Save
CubemapKTX2Transformer::SaveKTX2File(result, "output.ktx2");

// Cleanup
CubemapKTX2Transformer::DestroyTexture(source);
CubemapKTX2Transformer::DestroyTexture(result);
```

## Face Ordering

Faces are indexed 0-5:
- **0** = +X (right)
- **1** = -X (left)
- **2** = +Y (top)
- **3** = -Y (bottom)
- **4** = +Z (front)
- **5** = -Z (back)

Custom mappings use: `faceMapping[dst_position] = src_face_index`

## Engineering → OpenGL Transformation

Standard face reordering to convert Engineering axes to OpenGL:
```
+X stays +X
-X stays -X
+Y ← +Z  (new top is old front)
-Y ← -Z  (new bottom is old back)
+Z ← -Y  (new front is old bottom)
-Z ← +Y  (new back is old top)
```

## Dependencies
- **KTX library** (libb, libktx-dev on Linux)
- Standard C++ (uses std::vector, fstream)
- Teleport Logging (TELEPORT_WARN, TELEPORT_LOG)

## Building
Files are added to `TeleportServer/CMakeLists.txt`:
- `CubemapRearrangement.h` (header only, no separate compilation)
- `CubemapKTX2Transformer.cpp` (compiled into TeleportServer library)
- `CubemapRearrangementExample.cpp` (compiled for reference; can be removed)

## Performance
- **Load**: O(file_size) disk I/O
- **Transform**: O(texture_data_size) memory operations (fast, no decompression)
- **Save**: O(texture_data_size) disk I/O

All operations work on compressed KTX2 blocks, so transformation is very efficient.

## Limitations
- Face-level rearrangement only (not per-texel transformations)
- Requires cubemap input (will error on 2D textures)
- Preserves compression format (output format matches input)
