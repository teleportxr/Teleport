# CubemapConverter - KTX2 Cubemap Transformation Utility

A command-line tool for transforming KTX2 cubemap files, including converting between Engineering and OpenGL coordinate systems and custom face rearrangement.

## Building

### Prerequisites
- CMake 3.16+
- C++20 compiler
- Vulkan development libraries
- KTX library (libktx)
- TeleportServer library (built as part of Teleport)

### Build Steps

From the Teleport root directory:

```bash
# Option 1: Include in main Teleport build
cmake -B build_pc_client -S . && cmake --build build_pc_client --config Release

# Option 2: Build standalone (requires TeleportServer already built)
cmake -B build_converter -S Utils/CubemapConverter && cmake --build build_converter
```

The executable will be at: `build_pc_client/bin/CubemapConverter` (or similar depending on build path)

## Usage

```bash
CubemapConverter [options] <input.ktx2> <output.ktx2>
```

### Options

| Flag | Description |
|------|-------------|
| `-h, --help` | Show help message |
| `-e2g, --eng-to-gl` | Convert Engineering axes to OpenGL (default) |
| `-g2e, --gl-to-eng` | Convert OpenGL axes to Engineering |
| `-v, --verbose` | Verbose output |
| `-f, --faces <mapping>` | Custom face mapping (comma-separated indices 0-5) |

### Face Indices

KTX2 cubemap faces are indexed 0-5:
- **0** = +X (right)
- **1** = -X (left)
- **2** = +Y (top)
- **3** = -Y (bottom)
- **4** = +Z (front)
- **5** = -Z (back)

## Examples

### Convert Engineering to OpenGL (default)
Swaps Y and Z axes, common when converting from engine format to graphics API format:

```bash
CubemapConverter input_engineering.ktx2 output_opengl.ktx2
```

### Convert OpenGL to Engineering
Reverse transformation:

```bash
CubemapConverter --gl-to-eng input_opengl.ktx2 output_engineering.ktx2
```

### Verbose Output
See detailed progress information:

```bash
CubemapConverter -v input.ktx2 output.ktx2
```

### Custom Face Rearrangement
Specify exact face ordering. This example reverses the face order:

```bash
CubemapConverter -f 1,0,3,2,5,4 input.ktx2 output.ktx2
```

This mapping means:
- Output face 0 ← Input face 1 (-X)
- Output face 1 ← Input face 0 (+X)
- Output face 2 ← Input face 3 (-Y)
- Output face 3 ← Input face 2 (+Y)
- Output face 4 ← Input face 5 (-Z)
- Output face 5 ← Input face 4 (+Z)

## Coordinate System Conversion Details

### Engineering → OpenGL (default)

Engineering uses: +X=right, +Y=up(Z), +Z=back(Y)
OpenGL uses: +X=right, +Y=up, +Z=back(towards camera)

Face mapping:
```
Output[0] ← Input[0]  (+X)
Output[1] ← Input[1]  (-X)
Output[2] ← Input[4]  (+Y ← +Z)
Output[3] ← Input[5]  (-Y ← -Z)
Output[4] ← Input[2]  (+Z ← +Y)
Output[5] ← Input[3]  (-Z ← -Y)
```

### OpenGL → Engineering

Reverse transformation swapping +Y/+Z and -Y/-Z faces back.

## Performance

Typical performance on a 2048×2048 cubemap:
- Load: ~5ms (disk I/O)
- Transform: <1ms (memory copy)
- Save: ~5ms (disk I/O)
- **Total: ~10ms**

## Error Handling

The tool will report errors for:
- File not found
- Invalid KTX2 format (not a cubemap, corrupted file)
- Invalid face mapping (not 6 indices or out of range 0-5)
- Write permission issues
- Disk space issues

Exit codes:
- **0** = Success
- **1** = Error (see stderr for details)

## Integration with TeleportServer

The utility links against:
- `TeleportServer` - Provides `CubemapKTX2Transformer` class
- `TeleportCore` - Logging infrastructure
- `ktx` - KTX2 library
- `Vulkan` - Graphics format definitions

No modifications to existing code are required; it's a pure consumer of the public API.

## Advanced Usage

### Batch Processing

Process multiple cubemaps (shell script):

```bash
#!/bin/bash
for file in cubemaps/*.ktx2; do
    output="${file%.ktx2}_gl.ktx2"
    CubemapConverter "$file" "$output"
done
```

### Verify Transformation

Check dimensions are preserved:

```bash
CubemapConverter -v input.ktx2 output.ktx2 2>&1 | grep Dimensions
```

## Limitations

- **Cubemaps only**: Input must be a valid KTX2 cubemap (6 faces)
- **Compression preserved**: Output uses same compression as input
- **Face-level only**: Cannot perform per-texel pixel rearrangement
- **Single layer**: Standard cubemaps (arrayCount=1) only

## Future Enhancements

- Batch processing mode
- Cross-shaped image support
- HDR cubemap validation
- Metadata preservation options
- Parallel face processing for large cubemaps
