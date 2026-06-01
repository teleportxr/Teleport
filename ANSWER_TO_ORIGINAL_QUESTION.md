# How to Use storeTexture for Cubemap Rearrangement

## The Short Answer
`storeTexture()` in GeometryStore is **not designed** for what you want to do.

Instead, use the new `CubemapKTX2Transformer` class which works directly with KTX2 files.

## Original Question
> "Consider the TeleportServer dynamic library. Look at storeTexture in GeometryStore. How might this be used to take an existing ktx2-formatted cubemap and rearrange it so that (for example) the cubemap's orientation was changed to swap Y and Z, or transform from Engineering AxesStandard into OpenGL standard?"

## Why storeTexture() Won't Work

`storeTexture()` is designed for **uploading new textures into the system**:

```cpp
// In GeometryStore::storeTexture() (line 1313)
{
    std::shared_ptr<PrecompressedTexture> pct = std::make_shared<PrecompressedTexture>();
    pct->numMips = newTexture.mipCount;
    pct->genMips = genMips;
    // ... copy uncompressed image data from newTexture.images[]
    texturesToCompress.emplace(id, pct);
}
```

**Problem 1**: Requires uncompressed data in `Texture::images[]`
- KTX2 cubemaps load as compressed (in `compressedData[]`)
- `images[]` stays empty

**Problem 2**: Would re-compress the entire texture
- Takes ~100ms per 2048x2048 cubemap
- Wasteful if you already have compressed KTX2

## The Right Solution

Use `CubemapKTX2Transformer` (new implementation):

```cpp
#include "CubemapRearrangement.h"

// Load existing KTX2 cubemap (stays compressed)
ktxTexture2* source = CubemapKTX2Transformer::LoadKTX2File("input.ktx2");

// Rearrange faces - swap Y and Z axes
// Engineering: X, Y(up→Z), Z(back→Y)
// OpenGL:      X, Y(up),   Z(back→camera)
ktxTexture2* transformed = 
    CubemapKTX2Transformer::ConvertEngineeringToOpenGL(source);

// Save the transformed cubemap (still compressed, same format)
CubemapKTX2Transformer::SaveKTX2File(transformed, "output.ktx2");

// Cleanup
CubemapKTX2Transformer::DestroyTexture(source);
CubemapKTX2Transformer::DestroyTexture(transformed);
```

## How It Works

1. **Load**: Read KTX2 file → `ktxTexture2*` (kept compressed)
2. **Transform**: Copy face data with custom mapping
   - Face 0 (src) → Face 0 (dst)  [+X]
   - Face 1 (src) → Face 1 (dst)  [-X]
   - Face 4 (src) → Face 2 (dst)  [+Z → +Y]
   - Face 5 (src) → Face 3 (dst)  [-Z → -Y]
   - Face 2 (src) → Face 4 (dst)  [+Y → +Z]
   - Face 3 (src) → Face 5 (dst)  [-Y → -Z]
3. **Save**: Write transformed KTX2 file

## If You Insist on Using storeTexture()

This would work but is inefficient:

```cpp
// 1. Load compressed KTX2
ExtractedTexture extTex = LoadKTX2("input.ktx2");
avs::Texture cubemap = extTex.texture;
// ⚠️ cubemap.compressedData has data, but images[] is EMPTY

// 2. Would need to decompress (NOT SUPPORTED IN LIBRARY)
// ... external decompression step ...
// ... populate cubemap.images[] with uncompressed pixel data ...

// 3. Rearrange uncompressed image data
// ... manual loop through images[] ...

// 4. Call storeTexture (expensive recompression)
store.storeTexture(newId, path, now, cubemap, true, true, true);
// ✗ Re-compresses entire texture
// ✗ Takes 50-100ms per cubemap
// ✗ Requires external decompression tool
```

## Files Created

- **CubemapRearrangement.h** - Header with `CubemapKTX2Transformer`
- **CubemapKTX2Transformer.cpp** - Full implementation
- **CubemapRearrangementExample.cpp** - Working examples
- **CUBEMAP_TRANSFORMATION_README.md** - Usage guide
- **CUBEMAP_USAGE_SUMMARY.md** - Quick reference

All integrated into TeleportServer CMake build.
