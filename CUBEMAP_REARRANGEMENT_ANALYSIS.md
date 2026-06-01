# Cubemap Rearrangement with KTX2 Files

## Problem Statement
You have KTX2-formatted cubemap files (compressed, disk-resident). You need to:
1. Load them as KTX2 cubemaps
2. Rearrange faces (swap Y/Z axes, convert Engineering ↔ OpenGL standards)
3. Output as new KTX2 cubemap files

**Key constraint**: Work directly with KTX2 format without decompressing to uncompressed images.

## Current Pipeline Analysis

### Loading KTX2 Cubemaps
`LoadAsKtxFile()` (Texture.cpp:199) loads compressed KTX2:
```cpp
void LoadAsKtxFile(ExtractedTexture &extractedTexture,
                   const std::vector<char> &data,
                   const std::string &filename)
{
    // Reads KTX2 header via ktxTexture2 API
    ktxTexture2 *ktx2Texture = ...;
    avsTexture.width = ktx2Texture->baseWidth;
    avsTexture.height = ktx2Texture->baseHeight;
    avsTexture.cubemap = ktx2Texture->isCubemap;  // ← Sets flag
    avsTexture.compression = avs::TextureCompression::KTX;
    avsTexture.compressedData = <raw ktx2 bytes>;  // ← Stays compressed
    // Note: avsTexture.images[] remains EMPTY
}
```

### The Problem
`storeTexture()` → `CompressToKtx2()` (line 230) expects **uncompressed images** in `images[]`:
```cpp
// CompressToKtx2 loops through images[]:
for(uint32_t m=0; m<avsTexture.mipCount; m++)
    for(uint32_t l=0; l<avsTexture.arrayCount; l++)
        for(uint32_t face=0; face<createInfo.numFaces; face++)
        {
            std::vector<uint8_t> encodedImage = EncodeLayer(...);  // Requires images[]
```

**Solution**: Bypass `storeTexture()` and work with KTX2 directly via KTX library API.

## Recommended Approach: KTX Library Direct Manipulation

### Core Idea
1. **Load**: Use `ktxTexture_CreateFromMemory()` → get `ktxTexture2*`
2. **Rearrange**: Use KTX API to swap/copy image data between face indices
3. **Save**: Use `ktxTexture_WriteToMemory()` → output new KTX2 bytes

### Face Indexing in KTX
KTX stores cubemap faces with API: `ktxTexture_SetImageFromMemory(texture, m, layer, face, ...)`

Mapping:
- face 0 = +X, face 1 = -X
- face 2 = +Y, face 3 = -Y
- face 4 = +Z, face 5 = -Z

### Implementation Sketch
```cpp
// 1. Load source KTX2
ktxTexture2* src = nullptr;
ktxTexture_CreateFromMemory(compressedData.data(), compressedData.size(),
                            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &src);

// 2. Create new KTX2 with same spec
ktxTexture2* dst = nullptr;
ktxTextureCreateInfo info;
info.vkFormat = src->vkFormat;
info.baseWidth = src->baseWidth;
info.numFaces = 6;  // cubemap
// ... other fields from src
ktxTexture2_Create(&info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &dst);

// 3. Copy faces with transformation
// Example: swap Y/Z (faces 2↔4, 3↔5)
for(uint32_t m = 0; m < src->numLevels; m++) {
    CopyFace(src, dst, m, 0, 0);  // +X → +X
    CopyFace(src, dst, m, 1, 1);  // -X → -X
    CopyFace(src, dst, m, 4, 2);  // +Z → +Y (swap!)
    CopyFace(src, dst, m, 5, 3);  // -Z → -Y (swap!)
    CopyFace(src, dst, m, 2, 4);  // +Y → +Z (swap!)
    CopyFace(src, dst, m, 3, 5);  // -Y → -Z (swap!)
}

// 4. Write output KTX2
uint8_t* out = nullptr;
ktx_size_t outSize = 0;
ktxTexture_WriteToMemory(ktxTexture(dst), &out, &outSize);
// Save out to file
```

## Integration Point: New Helper Class

Create `CubemapKTX2Transformer`:
- `LoadKTX2(path)` → `ktxTexture2*`
- `TransformFaces(src, faceMapping)` → `ktxTexture2*`
- `SaveKTX2(dst, path)` → writes file

Keeps compressed data compressed throughout.

## Key Advantages
✓ Avoids decompression overhead
✓ Works with production KTX2 files
✓ Uses official KTX library API
✓ Preserves compression settings
✓ Fast in-memory transformation

## Key Limitations
✗ Doesn't integrate with GeometryStore.storeTexture()
✗ Requires direct KTX library calls
✗ Per-face rearrangement only (not per-texel transforms)
