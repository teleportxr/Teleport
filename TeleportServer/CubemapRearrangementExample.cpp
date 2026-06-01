// Example: Using CubemapKTX2Transformer to load, rearrange, and save KTX2 cubemaps
// Works directly with compressed KTX2 files - no decompression needed

#include "TeleportServer/CubemapRearrangement.h"
#include "TeleportCore/Logging.h"

namespace teleport::server
{
	// Example 1: Simple transformation - load KTX2, convert axes, save
	bool TransformCubemapFile(const std::string& inputPath,
							  const std::string& outputPath)
	{
		using CubemapKTX2 = CubemapKTX2Transformer;

		// Load the KTX2 cubemap from file
		ktxTexture2* source = CubemapKTX2::LoadKTX2File(inputPath);
		if (!source || !CubemapKTX2::IsCubemap(source)) {
			TELEPORT_WARN("Failed to load cubemap from {0}", inputPath);
			return false;
		}

		// Transform: Engineering axes → OpenGL axes (swap Y and Z)
		ktxTexture2* transformed = CubemapKTX2::ConvertEngineeringToOpenGL(source);
		if (!transformed) {
			TELEPORT_WARN("Failed to transform cubemap");
			CubemapKTX2::DestroyTexture(source);
			return false;
		}

		// Save the transformed cubemap to file
		bool success = CubemapKTX2::SaveKTX2File(transformed, outputPath);
		if (success) {
			TELEPORT_LOG("Successfully transformed cubemap {0} -> {1}",
						 inputPath, outputPath);
		} else {
			TELEPORT_WARN("Failed to save transformed cubemap to {0}", outputPath);
		}

		CubemapKTX2::DestroyTexture(source);
		CubemapKTX2::DestroyTexture(transformed);
		return success;
	}

	// Example 2: Custom face rearrangement
	bool CustomFaceRearrangement(const std::string& inputPath,
								 const std::string& outputPath)
	{
		using CubemapKTX2 = CubemapKTX2Transformer;
		using Face = CubemapKTX2::Face;

		ktxTexture2* source = CubemapKTX2::LoadKTX2File(inputPath);
		if (!source) return false;

		// Example: reverse face order (not a real transform, just demo)
		const Face customOrder[6] = {
			Face::NEG_X,  // dst[0] ← src face 1
			Face::POS_X,  // dst[1] ← src face 0
			Face::NEG_Y,  // dst[2] ← src face 3
			Face::POS_Y,  // dst[3] ← src face 2
			Face::NEG_Z,  // dst[4] ← src face 5
			Face::POS_Z   // dst[5] ← src face 4
		};

		ktxTexture2* transformed = CubemapKTX2::TransformFaceOrder(source, customOrder);
		if (!transformed) {
			TELEPORT_WARN("Face rearrangement failed");
			CubemapKTX2::DestroyTexture(source);
			return false;
		}

		bool success = CubemapKTX2::SaveKTX2File(transformed, outputPath);
		CubemapKTX2::DestroyTexture(source);
		CubemapKTX2::DestroyTexture(transformed);
		return success;
	}

	// Example 3: In-memory transformation (load, transform, get bytes)
	bool TransformCubemapInMemory(const std::vector<uint8_t>& inputKTX2Bytes,
								  std::vector<uint8_t>& outputKTX2Bytes)
	{
		using CubemapKTX2 = CubemapKTX2Transformer;

		// Load from memory
		ktxTexture2* source = CubemapKTX2::LoadKTX2Memory(
			inputKTX2Bytes.data(), inputKTX2Bytes.size());
		if (!source) {
			TELEPORT_WARN("Failed to load cubemap from memory");
			return false;
		}

		// Transform: OpenGL → Engineering (reverse transformation)
		ktxTexture2* transformed = CubemapKTX2::ConvertOpenGLToEngineering(source);
		if (!transformed) {
			TELEPORT_WARN("Transformation failed");
			CubemapKTX2::DestroyTexture(source);
			return false;
		}

		// Serialize to memory
		bool success = CubemapKTX2::SaveKTX2Memory(transformed, outputKTX2Bytes);
		if (success) {
			TELEPORT_LOG("In-memory transformation successful: {0} bytes -> {1} bytes",
						 inputKTX2Bytes.size(), outputKTX2Bytes.size());
		}

		CubemapKTX2::DestroyTexture(source);
		CubemapKTX2::DestroyTexture(transformed);
		return success;
	}

	// Example 4: Batch process multiple cubemap files
	bool TransformCubemapDirectory(const std::string& inputDir,
								   const std::string& outputDir)
	{
		// Example: iterate over .ktx2 files in inputDir
		// For each file: TransformCubemapFile(inputPath, outputPath)
		// This is pseudocode - implement using filesystem iteration

		TELEPORT_LOG("Batch transformation not implemented in example");
		return false;
	}
}

/*
USAGE FLOW:
-----------
1. Load KTX2 cubemap directly from file or memory
   ktxTexture2* src = CubemapKTX2Transformer::LoadKTX2File("scene_env.ktx2");

2. Transform (rearrange faces, swap axes)
   ktxTexture2* dst = CubemapKTX2Transformer::ConvertEngineeringToOpenGL(src);

3. Save transformed cubemap to file
   CubemapKTX2Transformer::SaveKTX2File(dst, "scene_env_gl.ktx2");

4. Clean up
   CubemapKTX2Transformer::DestroyTexture(src);
   CubemapKTX2Transformer::DestroyTexture(dst);

KEY ADVANTAGES:
- Works entirely with compressed KTX2 data
- No intermediate uncompressed images in memory
- Fast face rearrangement (just copying block data)
- Preserves all compression settings and metadata
- Can operate on files or in-memory buffers

TRANSFORMATIONS SUPPORTED:
- ConvertEngineeringToOpenGL() - Swap Y/Z axes, adjust face order
- ConvertOpenGLToEngineering() - Reverse of above
- TransformFaceOrder() - Custom face rearrangement with arbitrary mapping
*/
