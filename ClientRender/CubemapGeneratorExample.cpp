// Example usage of CubemapGenerator
// This file demonstrates how to use the CubemapGenerator class to create
// cubemaps using the unconnected shaders and save them to KTX2 format.

#include "CubemapGenerator.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "TeleportCore/ErrorHandling.h"

using namespace teleport::clientrender;
using namespace platform::crossplatform;

/// Example function showing how to generate and save cubemaps
/// @param renderPlatform The render platform to use for rendering
/// @param outputPath Directory where to save the generated cubemaps
/// @return True if successful
bool GenerateExampleCubemaps(RenderPlatform* renderPlatform, const std::string& outputPath)
{
	if (!renderPlatform)
	{
		TELEPORT_CERR << "No render platform provided" << std::endl;
		return false;
	}

	// Create the cubemap generator
	CubemapGenerator generator(renderPlatform);

	// Initialize the generator
	if (!generator.Initialize())
	{
		TELEPORT_CERR << "Failed to initialize CubemapGenerator" << std::endl;
		return false;
	}

	// Generate a white-themed cubemap
	TELEPORT_COUT << "Generating white cubemap..." << std::endl;
	if (!generator.GenerateCubemap("white", 512, 0.0f))
	{
		TELEPORT_CERR << "Failed to generate white cubemap" << std::endl;
		return false;
	}

	// Save the white cubemap
	std::string whiteFilename = outputPath + "/unconnected_white_cubemap.hdr";
	if (!generator.SaveToHDR(whiteFilename))
	{
		TELEPORT_CERR << "Failed to save white cubemap" << std::endl;
		return false;
	}

	// Generate a neon-themed cubemap with animation
	TELEPORT_COUT << "Generating neon cubemap..." << std::endl;
	if (!generator.GenerateCubemap("neon", 512, 5.0f)) // 5 seconds for animation
	{
		TELEPORT_CERR << "Failed to generate neon cubemap" << std::endl;
		return false;
	}

	// Save the neon cubemap
	std::string neonFilename = outputPath + "/unconnected_neon_cubemap.hdr";
	if (!generator.SaveToHDR(neonFilename))
	{
		TELEPORT_CERR << "Failed to save neon cubemap" << std::endl;
		return false;
	}

	// Generate a higher resolution neon cubemap
	TELEPORT_COUT << "Generating high-res neon cubemap..." << std::endl;
	if (!generator.GenerateCubemap("neon", 1024, 10.0f)) // 10 seconds for different animation phase
	{
		TELEPORT_CERR << "Failed to generate high-res neon cubemap" << std::endl;
		return false;
	}

	// Save the high-res neon cubemap
	std::string neonHiResFilename = outputPath + "/unconnected_neon_cubemap_1024.hdr";
	if (!generator.SaveToHDR(neonHiResFilename))
	{
		TELEPORT_CERR << "Failed to save high-res neon cubemap" << std::endl;
		return false;
	}

	TELEPORT_COUT << "Successfully generated all cubemaps!" << std::endl;
	return true;
}

/// Example function showing how to generate animated sequence of cubemaps
/// @param renderPlatform The render platform to use for rendering
/// @param outputPath Directory where to save the generated cubemaps
/// @param frameCount Number of frames to generate
/// @param duration Total duration in seconds
/// @return True if successful
bool GenerateAnimatedCubemapSequence(RenderPlatform* renderPlatform, const std::string& outputPath, 
									 int frameCount, float duration)
{
	if (!renderPlatform)
	{
		TELEPORT_CERR << "No render platform provided" << std::endl;
		return false;
	}

	// Create the cubemap generator
	CubemapGenerator generator(renderPlatform);

	// Initialize the generator
	if (!generator.Initialize())
	{
		TELEPORT_CERR << "Failed to initialize CubemapGenerator" << std::endl;
		return false;
	}

	TELEPORT_COUT << "Generating animated cubemap sequence..." << std::endl;

	for (int frame = 0; frame < frameCount; ++frame)
	{
		// Calculate time for this frame
		float timeSeconds = (duration * frame) / frameCount;

		// Generate neon cubemap for this frame
		if (!generator.GenerateCubemap("neon", 512, timeSeconds))
		{
			TELEPORT_CERR << "Failed to generate cubemap for frame " << frame << std::endl;
			return false;
		}

		// Save this frame
		char frameFilename[256];
		snprintf(frameFilename, sizeof(frameFilename), "%s/neon_cubemap_frame_%04d.hdr",
				 outputPath.c_str(), frame);

		if (!generator.SaveToHDR(frameFilename))
		{
			TELEPORT_CERR << "Failed to save cubemap frame " << frame << std::endl;
			return false;
		}

		// Progress indicator
		if (frame % 10 == 0)
		{
			TELEPORT_COUT << "Generated frame " << frame << "/" << frameCount << std::endl;
		}
	}

	TELEPORT_COUT << "Successfully generated animated cubemap sequence!" << std::endl;
	return true;
}

/// Example function showing how to use the generator in a rendering loop
/// This could be called from your main rendering code
void ExampleUsageInRenderLoop(RenderPlatform* renderPlatform)
{
	static CubemapGenerator* generator = nullptr;
	static bool initialized = false;

	// Initialize once
	if (!initialized)
	{
		generator = new CubemapGenerator(renderPlatform);
		if (generator->Initialize())
		{
			initialized = true;
			TELEPORT_COUT << "CubemapGenerator initialized for runtime use" << std::endl;
		}
		else
		{
			delete generator;
			generator = nullptr;
			TELEPORT_CERR << "Failed to initialize CubemapGenerator" << std::endl;
			return;
		}
	}

	// Example: Generate a cubemap based on current time
	if (generator)
	{
		static float lastGenerationTime = 0.0f;
		float currentTime = 0.0f; // Get current time from your time system
		
		// Generate a new cubemap every 5 seconds
		if (currentTime - lastGenerationTime > 5.0f)
		{
			if (generator->GenerateCubemap("neon", 256, currentTime))
			{
				// You can now use generator->GetCubemapTexture() for rendering
				// or save it to file
				char filename[256];
				snprintf(filename, sizeof(filename), "runtime_cubemap_%.1f.hdr", currentTime);
				generator->SaveToHDR(filename);
				
				lastGenerationTime = currentTime;
			}
		}
	}
}

// Cleanup function - call this when shutting down
void CleanupCubemapGenerator()
{
	// If you have a static generator instance, clean it up here
	// This is just an example - adapt to your memory management strategy
}
