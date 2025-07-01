# CubemapGenerator

The `CubemapGenerator` class provides functionality to generate cubemaps using the "unconnected" shaders from `cubemap_clear.sfx` and save them as HDR cubemap cross images.

## Overview

This class renders the same background effects that are shown in the Teleport client when not connected to a server, but captures them into an HDR cubemap cross layout that can be saved and reused.

## Features

- Generate cubemaps using "white" or "neon" unconnected shader passes
- Configurable cubemap face resolution (e.g., 512x512, 1024x1024 per face)
- Time-based animation support for animated effects
- Save generated cubemaps as HDR cubemap cross (4x3 layout)
- Platform-agnostic using the Platform rendering API

## Usage

### Basic Usage

```cpp
#include "CubemapGenerator.h"

// Create generator with your render platform
CubemapGenerator generator(renderPlatform);

// Initialize the generator
if (!generator.Initialize())
{
    // Handle initialization error
    return false;
}

// Generate a white-themed cubemap (512x512 per face) using Engineering standard
if (!generator.GenerateCubemap(deviceContext, "white", 512, platform::crossplatform::AxesStandard::Engineering, 0.0f))
{
    // Handle generation error
    return false;
}

// Save to HDR file
if (!generator.SaveToHDR(deviceContext, "white_cubemap.hdr"))
{
    // Handle save error
    return false;
}
```

### Different Coordinate Systems

```cpp
// Generate for OpenGL (Y vertical, right-handed)
generator.GenerateCubemap(deviceContext, "white", 512, platform::crossplatform::AxesStandard::OpenGL);

// Generate for Unreal (Z vertical, left-handed)
generator.GenerateCubemap(deviceContext, "white", 512, platform::crossplatform::AxesStandard::Unreal);

// Generate for Unity (Y vertical, left-handed)
generator.GenerateCubemap(deviceContext, "white", 512, platform::crossplatform::AxesStandard::Unity);

// Generate for Engineering (Z vertical, right-handed) - default
generator.GenerateCubemap(deviceContext, "white", 512, platform::crossplatform::AxesStandard::Engineering);
```

### Animated Cubemaps

```cpp
// Generate neon cubemap with animation at 5.0 seconds
generator.GenerateCubemap(deviceContext, "neon", 512, platform::crossplatform::AxesStandard::Engineering, 5.0f);
generator.SaveToHDR(deviceContext, "neon_animated.hdr");

// Generate sequence for animation
for (int frame = 0; frame < 60; ++frame)
{
    float timeSeconds = frame * 0.1f; // 0.1 second per frame
    generator.GenerateCubemap(deviceContext, "neon", 512, platform::crossplatform::AxesStandard::Engineering, timeSeconds);

    char filename[256];
    sprintf(filename, "neon_frame_%04d.hdr", frame);
    generator.SaveToHDR(deviceContext, filename);
}
```

### Available Shader Passes

- **"white"**: Clean white grid pattern with subtle animations
- **"neon"**: Colorful neon grid with pulsing effects and fractal patterns

## API Reference

### Constructor
```cpp
CubemapGenerator(platform::crossplatform::RenderPlatform* renderPlatform)
```

### Methods

#### Initialize()
```cpp
bool Initialize()
```
Initializes the generator, loads shaders, and creates constant buffers.
Returns `true` on success.

#### GenerateCubemap()
```cpp
bool GenerateCubemap(const std::string& passName, int cubemapSize, float timeSeconds = 0.0f)
```
- `passName`: Either "white" or "neon"
- `cubemapSize`: Resolution per face (e.g., 512, 1024)
- `timeSeconds`: Time value for animation effects
Returns `true` on success.

#### SaveToHDR()
```cpp
bool SaveToHDR(const std::string& filename)
```
Saves the generated cubemap to an HDR file as a cubemap cross layout.
Returns `true` on success.

#### GetCubemapTexture()
```cpp
platform::crossplatform::Texture* GetCubemapTexture() const
```
Returns the generated cubemap texture for direct use in rendering.

## Technical Details

### Shader Integration

The generator uses the existing `cubemap_clear.sfx` shader with the "unconnected" technique. This ensures consistency with the visual effects shown in the client application.

### Camera and Cubemap Constants

The generator properly sets up both CameraConstants and CubemapConstants for each face:
- **CameraConstants**: Contains view, projection, and inverse view-projection matrices for each cubemap face
- **CubemapConstants**: Contains timing, camera position, and rendering parameters
- Uses `crossplatform::GetCubeInvViewProjMatrix()` to get proper face matrices
- Each face gets a 90-degree FOV projection matrix appropriate for cubemap rendering

### Coordinate System Support

The generator supports multiple coordinate systems through the AxesStandard parameter:

- **Engineering** (Z vertical, right-handed): Default API standard
- **OpenGL** (Y vertical, right-handed): Standard OpenGL coordinate system
- **Unreal** (Z vertical, left-handed): Unreal Engine coordinate system
- **Unity** (Y vertical, left-handed): Unity Engine coordinate system

The coordinate system transformation is handled through the matrices:
- The HDR cross layout always uses the standard Engineering face arrangement
- Inverse view-projection matrices are generated specifically for each coordinate system
- Coordinate system differences are handled entirely through the `invViewProj` matrices
- The physical layout in the HDR file remains consistent regardless of coordinate system

### HDR Cubemap Cross Layout

The generator renders each of the 6 cubemap faces to specific viewports in a cross layout:
```
    +Y
-X  +Z  +X  -Z
    -Y
```
- Each face is rendered to its designated position in the 4x3 grid
- Final image dimensions: faceSize * 4 × faceSize * 3
- Uses proper cubemap view matrices and inverse view-projection matrices for each face

### HDR Output

Generated cubemaps are saved in HDR format with:
- RGBA_16_FLOAT pixel format during rendering
- RGBE format in the final HDR file
- Cubemap cross layout (4x3 arrangement)
- High dynamic range support for bright effects

### Memory Management

The class uses RAII principles with smart pointers for automatic resource cleanup. Textures and effects are automatically released when the generator is destroyed.

## Dependencies

- Platform rendering API with HDR SaveTexture support
- STB image library (for HDR file writing)
- cubemap_clear.sfx shader file
- cubemap_constants.sl constant buffer definitions

## Example Files

- `CubemapGeneratorExample.cpp/h`: Complete usage examples
- Shows batch generation, animation sequences, and runtime usage patterns

## Error Handling

All methods return boolean success/failure indicators. Error messages are logged using the Teleport logging system (`TELEPORT_CERR`, `TELEPORT_COUT`).

## Performance Considerations

- Larger cubemap face sizes (1024+) require more GPU memory and rendering time
- Consider generating cubemaps offline for production use
- The staging texture copy for HDR export may be slow for large textures
- HDR cross layout requires 4x3 times the memory of a single face
- Multiple cubemap generations should reuse the same generator instance

## Integration Notes

This class is designed to integrate seamlessly with the existing Teleport rendering pipeline. It uses the same shaders and rendering techniques as the main client application, ensuring visual consistency.
