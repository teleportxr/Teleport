#pragma once

#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/AxesStandard.h"
#include "client/Shaders/cubemap_constants.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
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
			/// @param deviceContext Graphics device context
			/// @param passName Either "white" or "neon"
			/// @param cubemapSize Size of each face of the cubemap (e.g., 512, 1024)
			/// @param axesStandard Coordinate system standard for face layout and orientation
			/// @param timeSeconds Time value for animated effects
			/// @return True if generation was successful
			bool GenerateCubemap(platform::crossplatform::GraphicsDeviceContext &deviceContext,
								 const std::string& passName,
								 int cubemapSize,
								 platform::crossplatform::AxesStandard axesStandard = platform::crossplatform::AxesStandard::Engineering,
								 float timeSeconds = 0.0f);

			/// Save the generated cubemap to an HDR file as a cubemap cross
			/// @param filename Path where to save the HDR file
			/// @return True if save was successful
			bool SaveToHDR(platform::crossplatform::GraphicsDeviceContext &deviceContext, const std::string& filename);

			/// Get the generated cubemap texture (for preview or further use)
			platform::crossplatform::Texture* GetCubemapTexture() const { return m_hdrCrossTexture.get(); }

		private:
			void Cleanup();
			bool CreateHDRCrossTexture(int faceSize);
			bool LoadShaders();
			void SetupConstants(float timeSeconds);
			void SetupCameraConstants(int face, vec3 viewPosition, platform::crossplatform::AxesStandard axesStandard);
			void RenderFaceToViewport(platform::crossplatform::GraphicsDeviceContext& deviceContext,
									  int face, int faceSize, const std::string& passName,
									  platform::crossplatform::AxesStandard axesStandard, float timeSeconds);
			void GetFaceViewportPosition(int face, int faceSize, platform::crossplatform::AxesStandard axesStandard,
										 int& viewportX, int& viewportY);

			platform::crossplatform::RenderPlatform* m_renderPlatform;
			std::shared_ptr<platform::crossplatform::Effect> m_cubemapClearEffect;
			std::shared_ptr<platform::crossplatform::Texture> m_hdrCrossTexture;
			platform::crossplatform::ConstantBuffer<CubemapConstants> m_cubemapConstants;
			platform::crossplatform::ConstantBuffer<CameraConstants> m_cameraConstants;

			int m_faceSize;
			bool m_initialized;
		};
	}
}
