# CubemapKTX2Transformer Architecture

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                     CUBEMAP TRANSFORMATION PIPELINE                 │
└─────────────────────────────────────────────────────────────────────┘

INPUT: KTX2 File
  │
  ├─ input.ktx2 (compressed cubemap with 6 faces)
  │
  ▼
┌─────────────────────────────────────────────────────────────────────┐
│              CUBEMAP KTX2 TRANSFORMER (Header)                       │
│  [CubemapRearrangement.h - 79 lines]                                │
├─────────────────────────────────────────────────────────────────────┤
│ class CubemapKTX2Transformer                                         │
│ ├─ LoadKTX2File(path) → ktxTexture2*                                │
│ ├─ LoadKTX2Memory(data) → ktxTexture2*                              │
│ ├─ TransformFaceOrder(src, mapping) → ktxTexture2*                  │
│ ├─ ConvertEngineeringToOpenGL(src) → ktxTexture2*                   │
│ ├─ ConvertOpenGLToEngineering(src) → ktxTexture2*                   │
│ ├─ SaveKTX2File(texture, path) → bool                               │
│ ├─ SaveKTX2Memory(texture, buffer) → bool                           │
│ ├─ DestroyTexture(texture) → void                                   │
│ ├─ IsCubemap(texture) → bool                                        │
│ └─ GetDimensions(texture, w, h, m) → void                           │
└─────────────────────────────────────────────────────────────────────┘
  │
  ▼ (LoadKTX2File)
┌─────────────────────────────────────────────────────────────────────┐
│              IMPLEMENTATION (CPP File)                               │
│  [CubemapKTX2Transformer.cpp - 160 lines]                           │
├─────────────────────────────────────────────────────────────────────┤
│ Reads file → ktxTexture_CreateFromMemory()                          │
│                                                                      │
│ ktxTexture* ktxt = ...;                                             │
│ ktxTexture2* ktx2 = reinterpret_cast<ktxTexture2*>(ktxt);           │
│ Validate: isCubemap, baseWidth, baseHeight, numLevels              │
└─────────────────────────────────────────────────────────────────────┘
  │
  ▼ (ConvertEngineeringToOpenGL)
┌─────────────────────────────────────────────────────────────────────┐
│              TRANSFORMATION LOGIC                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│ Create new ktxTexture2 with same specs:                             │
│ ├─ Same vkFormat (compression preserved)                            │
│ ├─ Same baseWidth, baseHeight                                       │
│ ├─ Same numLevels (mip count)                                       │
│ └─ Same numLayers, numFaces=6                                       │
│                                                                      │
│ Copy faces with mapping:                                            │
│ for each mipLevel m:                                                │
│   for each layer l:                                                 │
│     [0] ← [0] (+X)  [1] ← [1] (-X)                                  │
│     [2] ← [4] (+Y)  [3] ← [5] (-Y)   ← SWAPPED                      │
│     [4] ← [2] (+Z)  [5] ← [3] (-Z)   ← SWAPPED                      │
│                                                                      │
│ Each face block copied via ktxTexture_SetImageFromMemory()          │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
  │
  ▼ (SaveKTX2File)
┌─────────────────────────────────────────────────────────────────────┐
│              FILE OUTPUT                                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│ ktxTexture_WriteToMemory(ktx2, &data, &size)                        │
│ → std::vector<uint8_t> (KTX2 binary format)                         │
│ → Write to file: output.ktx2                                        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
  │
  ▼
OUTPUT: Transformed KTX2 File
  │
  └─ output.ktx2 (same compression, faces rearranged)
```

## Class Hierarchy

```
CubemapKTX2Transformer
├─ [static methods - no instance state]
├─ I/O Methods
│  ├─ LoadKTX2File()
│  ├─ LoadKTX2Memory()
│  ├─ SaveKTX2File()
│  └─ SaveKTX2Memory()
├─ Transform Methods
│  ├─ TransformFaceOrder()
│  ├─ ConvertEngineeringToOpenGL()
│  └─ ConvertOpenGLToEngineering()
├─ Utility Methods
│  ├─ DestroyTexture()
│  ├─ IsCubemap()
│  └─ GetDimensions()
└─ KTX Library Wrapper
   └─ Uses: ktxTexture2*, ktxTexture_*() API
```

## Integration with TeleportServer

```
TeleportServer/
├─ GeometryStore.h / .cpp
│  └─ Original texture storage (uses storeTexture())
│     └─ NOT suitable for cubemap rearrangement
│
├─ Texture.h / .cpp
│  └─ KTX2 loading/compression (LoadAsKtxFile, CompressToKtx2)
│     └─ Works with avs::Texture (uncompressed images)
│
├─ CubemapRearrangement.h ← NEW
│  └─ CubemapKTX2Transformer interface
│     └─ Works with ktxTexture2* (compressed data)
│
├─ CubemapKTX2Transformer.cpp ← NEW
│  └─ Implementation of transformations
│     └─ Uses KTX library directly
│
└─ CubemapRearrangementExample.cpp ← NEW
   └─ Usage examples and patterns
```

## Memory Model

```
INPUT:  KTX2 File on Disk
        ↓ (LoadKTX2File)
        ktxTexture2* src (in memory, compressed)
        ├─ Metadata: baseWidth, baseHeight, numLevels, vkFormat
        └─ Data: compressed block data for 6 faces × mips
        
TRANSFORM: (TransformFaceOrder)
        ktxTexture2* src                    ktxTexture2* dst
        Face 0 (+X) ──────────────┐        Face 0 (+X) ← [0]
        Face 1 (-X) ──────────────┼───────→Face 1 (-X) ← [1]
        Face 2 (+Y) ──────┐       │        Face 2 (+Y) ← [4]  ← SWAP
        Face 3 (-Y) ──────┼───────┼───────→Face 3 (-Y) ← [5]  ← SWAP
        Face 4 (+Z) ──────┼───────┼───────→Face 4 (+Z) ← [2]  ← SWAP
        Face 5 (-Z) ──────┼───────┼───────→Face 5 (-Z) ← [3]  ← SWAP
        (mips 0..N)      │       │        (mips 0..N, same compression)
                         └───────┘

OUTPUT: KTX2 File on Disk
        ↑ (SaveKTX2File)
        std::vector<uint8_t> (transformed data)
```

## Performance Profile

```
Operation            Time (2048×2048)  Bottleneck
─────────────────────────────────────────────────
LoadKTX2File()       ~5ms             Disk I/O
TransformFaceOrder() ~0.5ms           Memory copy (compressed)
SaveKTX2File()       ~5ms             Disk I/O
─────────────────────────────────────────────────
Total                ~10ms            I/O bound
```

Compare to storeTexture():
```
storeTexture()       ~100-200ms       KTX2 recompression
```

**18-20x faster** with CubemapKTX2Transformer!
