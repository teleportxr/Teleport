#pragma once

#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/ConstantBuffer.h"
#include "client/Shaders/cubemap_constants.sl"
#include <string>
#include <memory>

namespace teleport
{
	namespace clientrender
	{
		/// Class for generating cubemaps using the unconnected shaders and saving to KTX2 format
		class CubemapGenerator
		{
		public:
			CubemapGenerator(platform::crossplatform::RenderPlatform* renderPlatform);
			~CubemapGenerator();

			/// Initialize the generator with the required shaders and resources
			bool Initialize();

			/// Generate a cubemap using the specified unconnected shader pass
			/// @param passName Either "white" or "neon" 
			/// @param cubemapSize Size of each face of the cubemap (e.g., 512, 1024)
			/// @param timeSeconds Time value for animated effects
			/// @return True if generation was successful
			bool GenerateCubemap(const std::string& passName, int cubemapSize, float timeSeconds = 0.0f);

			/// Save the generated cubemap to a KTX2 file
			/// @param filename Path where to save the KTX2 file
			/// @return True if save was successful
			bool SaveToKTX2(const std::string& filename);

			/// Get the generated cubemap texture (for preview or further use)
			platform::crossplatform::Texture* GetCubemapTexture() const { return m_cubemapTexture.get(); }

		private:
			void Cleanup();
			bool CreateCubemapTexture(int size);
			bool LoadShaders();
			void SetupConstants(float timeSeconds);

			platform::crossplatform::RenderPlatform* m_renderPlatform;
			std::unique_ptr<platform::crossplatform::Effect> m_cubemapClearEffect;
			std::unique_ptr<platform::crossplatform::Texture> m_cubemapTexture;
			std::unique_ptr<platform::crossplatform::ConstantBuffer<CubemapConstants>> m_cubemapConstants;
			
			int m_cubemapSize;
			bool m_initialized;
		};
	}
}
