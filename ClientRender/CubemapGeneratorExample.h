#pragma once

#include <string>

namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
}

/// Example functions demonstrating CubemapGenerator usage

/// Generate example cubemaps using both white and neon passes
/// @param renderPlatform The render platform to use for rendering
/// @param outputPath Directory where to save the generated cubemaps
/// @return True if successful
bool GenerateExampleCubemaps(platform::crossplatform::RenderPlatform* renderPlatform, 
							 const std::string& outputPath);

/// Generate an animated sequence of cubemaps
/// @param renderPlatform The render platform to use for rendering
/// @param outputPath Directory where to save the generated cubemaps
/// @param frameCount Number of frames to generate
/// @param duration Total duration in seconds
/// @return True if successful
bool GenerateAnimatedCubemapSequence(platform::crossplatform::RenderPlatform* renderPlatform, 
									 const std::string& outputPath, 
									 int frameCount, 
									 float duration);

/// Example of using the generator in a runtime rendering loop
/// @param renderPlatform The render platform to use for rendering
void ExampleUsageInRenderLoop(platform::crossplatform::RenderPlatform* renderPlatform);

/// Cleanup function for static generator instances
void CleanupCubemapGenerator();
