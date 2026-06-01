#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <ktx.h>

namespace teleport::server
{
	/// Helper class for rearranging KTX2 cubemap files.
	/// Works directly with compressed KTX2 data without decompression.
	/// Transforms cubemaps between coordinate systems (e.g., Engineering ↔ OpenGL).
	class CubemapKTX2Transformer
	{
	public:
		/// Cubemap face indices as defined by KTX library
		enum Face : uint32_t {
			POS_X = 0,  // +X right
			NEG_X = 1,  // -X left
			POS_Y = 2,  // +Y top
			NEG_Y = 3,  // -Y bottom
			POS_Z = 4,  // +Z front
			NEG_Z = 5   // -Z back
		};

		/// Load KTX2 cubemap from file
		/// @param path Path to .ktx2 file
		/// @return Pointer to ktxTexture2, or nullptr if load fails
		/// @note Caller owns the returned pointer; use DestroyTexture() to free
		static ktxTexture2* LoadKTX2File(const std::string& path);

		/// Load KTX2 cubemap from memory
		/// @param data Pointer to KTX2 file bytes
		/// @param size Size of data in bytes
		/// @return Pointer to ktxTexture2, or nullptr if load fails
		static ktxTexture2* LoadKTX2Memory(const uint8_t* data, size_t size);

		/// Create a transformed copy of a cubemap by rearranging faces
		/// @param source Source ktxTexture2 (must be a cubemap)
		/// @param faceMapping Array of 6 face indices defining the new order
		///                    faceMapping[dst] = src (which source face goes to dst position)
		/// @return New ktxTexture2* with rearranged faces, or nullptr on error
		/// @note Caller owns the returned pointer
		static ktxTexture2* TransformFaceOrder(const ktxTexture2* source,
											   const Face faceMapping[6]);

		/// Convert cubemap from Engineering axes to OpenGL axes
		/// Engineering: +X=right, +Y=up(Z), +Z=back(Y)
		/// OpenGL:      +X=right, +Y=up,    +Z=back(towards camera)
		/// @param source Source ktxTexture2 cubemap
		/// @return Transformed cubemap with Y/Z swapped
		static ktxTexture2* ConvertEngineeringToOpenGL(const ktxTexture2* source);

		/// Convert cubemap from OpenGL axes to Engineering axes (inverse)
		static ktxTexture2* ConvertOpenGLToEngineering(const ktxTexture2* source);

		/// Save KTX2 cubemap to file
		/// @param texture ktxTexture2 to save
		/// @param path Output file path (.ktx2)
		/// @return true if successful
		static bool SaveKTX2File(const ktxTexture2* texture, const std::string& path);

		/// Extract KTX2 cubemap to memory buffer
		/// @param texture ktxTexture2 to serialize
		/// @param outBuffer Vector to receive KTX2 file bytes
		/// @return true if successful
		static bool SaveKTX2Memory(const ktxTexture2* texture, std::vector<uint8_t>& outBuffer);

		/// Free a ktxTexture2 pointer
		static void DestroyTexture(ktxTexture2* texture);

		/// Check if a ktxTexture2 is a valid cubemap
		static bool IsCubemap(const ktxTexture2* texture);

		/// Get texture dimensions
		static void GetDimensions(const ktxTexture2* texture,
								 uint32_t& width, uint32_t& height, uint32_t& mipCount);
	};
}
