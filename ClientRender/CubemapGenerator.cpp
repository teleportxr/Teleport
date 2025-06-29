#include "CubemapGenerator.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/Math/Vector3.h"
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Math/Pi.h"
#include "TeleportCore/ErrorHandling.h"

using namespace teleport::clientrender;
using namespace platform::crossplatform;
using namespace platform::math;

CubemapGenerator::CubemapGenerator(RenderPlatform* renderPlatform)
	: m_renderPlatform(renderPlatform)
	, m_faceSize(0)
	, m_initialized(false)
{
}

CubemapGenerator::~CubemapGenerator()
{
	Cleanup();
}

bool CubemapGenerator::Initialize()
{
	if (m_initialized)
		return true;

	if (!m_renderPlatform)
	{
		TELEPORT_CERR << "CubemapGenerator: No render platform provided" << std::endl;
		return false;
	}

	// Load the cubemap_clear effect
	if (!LoadShaders())
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to load shaders" << std::endl;
		return false;
	}
	m_cubemapConstants.RestoreDeviceObjects(m_renderPlatform);
	m_cameraConstants.RestoreDeviceObjects(m_renderPlatform);
	m_initialized = true;
	return true;
}

bool CubemapGenerator::LoadShaders()
{
	// Load the cubemap_clear.sfx effect
	m_cubemapClearEffect=m_renderPlatform->GetOrCreateEffect("cubemap_clear");
	if (!m_cubemapClearEffect)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create cubemap_clear effect" << std::endl;
		return false;
	}

	return true;
}

bool CubemapGenerator::CreateHDRCrossTexture(int faceSize)
{
	if (m_hdrCrossTexture && m_faceSize == faceSize)
		return true; // Already created with correct size

	m_hdrCrossTexture.reset();
	m_faceSize = faceSize;

	// Create HDR cross texture (4 faces wide x 3 faces tall)
	int crossWidth = faceSize * 4;
	int crossHeight = faceSize * 3;

	m_hdrCrossTexture = std::unique_ptr<Texture>(m_renderPlatform->CreateTexture());
	if (!m_hdrCrossTexture)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create HDR cross texture" << std::endl;
		return false;
	}

	// Configure as HDR render target
	TextureCreate textureCreate;
	textureCreate.w = crossWidth;
	textureCreate.l = crossHeight;
	textureCreate.d = 1;
	textureCreate.arraysize = 1;
	textureCreate.mips = 1;
	textureCreate.cubemap = false;
	textureCreate.f = PixelFormat::RGBA_16_FLOAT; // HDR format
	textureCreate.make_rt = true;
	textureCreate.computable = true;
	textureCreate.name = "HDRCubemapCross";

	if (!m_hdrCrossTexture->EnsureTexture(m_renderPlatform, &textureCreate))
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create HDR cross texture" << std::endl;
		return false;
	}

	return true;
}

void CubemapGenerator::SetupConstants(float timeSeconds)
{
	// Set time for animated effects
	m_cubemapConstants.time_seconds = timeSeconds;

	// Set up identity matrices and default values
	m_cubemapConstants.serverProj = mat4::identity();
	m_cubemapConstants.cameraPosition = vec3(0.0f, 0.0f, 0.0f); // Origin for cubemap generation
	m_cubemapConstants.cameraRotation = vec4(0.0f, 0.0f, 0.0f, 1.0f); // Identity quaternion
	m_cubemapConstants.depthOffsetScale = vec4(0.0f, 0.0f, 1.0f, 1.0f);
	m_cubemapConstants.targetSize = int2(m_faceSize, m_faceSize);
	m_cubemapConstants.sourceOffset = int2(0, 0);
	m_cubemapConstants.offsetFromVideo = vec3(0.0f, 0.0f, 0.0f);
}

void CubemapGenerator::SetupCameraConstants(int face)
{
	// Get the inverse view-projection matrix for this cubemap face
	platform::math::Matrix4x4 invViewProj;
	platform::crossplatform::GetCubeInvViewProjMatrix((float*)&invViewProj, face, true, false);

	// Set up camera constants
	m_cameraConstants.invViewProj = invViewProj;

	// Get the view matrix for this face
	platform::math::Matrix4x4 viewMatrix;
	platform::crossplatform::GetCubeMatrix((float*)&viewMatrix, face, true, false);
	m_cameraConstants.view = viewMatrix;

	// Create a 90-degree projection matrix for cubemap faces
	float fov = SIMUL_PI_F / 2.0f; // 90 degrees
	platform::math::Matrix4x4 projMatrix = platform::crossplatform::Camera::MakeDepthReversedProjectionMatrix(fov, fov, 0.1f, 1000.0f);
	m_cameraConstants.proj = projMatrix;

	// Calculate view-projection matrix
	platform::math::Matrix4x4 viewProj;
	platform::math::Multiply4x4(viewProj, viewMatrix, projMatrix);
	m_cameraConstants.viewProj = viewProj;

	// Set camera position at origin for cubemap generation
	m_cameraConstants.viewPosition = vec3(0.0f, 0.0f, 0.0f);

	// Set frame number (can be used for temporal effects)
	m_cameraConstants.frameNumber = 0;

	// Set depth parameters (these may need adjustment based on your needs)
	m_cameraConstants.depthToLinFadeDistParams = vec4(0.1f, 1000.0f, 0.0f, 0.0f);
}

bool CubemapGenerator::GenerateCubemap(platform::crossplatform::GraphicsDeviceContext &deviceContext,const std::string& passName, int cubemapSize, float timeSeconds)
{
	if (!m_initialized)
	{
		TELEPORT_CERR << "CubemapGenerator: Not initialized" << std::endl;
		return false;
	}

	if (passName != "white" && passName != "neon")
	{
		TELEPORT_CERR << "CubemapGenerator: Invalid pass name. Use 'white' or 'neon'" << std::endl;
		return false;
	}

	// Create HDR cross texture
	if (!CreateHDRCrossTexture(cubemapSize))
		return false;

	// Activate render target
	m_hdrCrossTexture->activateRenderTarget(deviceContext);

	// Clear the HDR cross texture
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_renderPlatform->Clear(deviceContext, clearColor);

	// Render each face to its position in the cross layout
	for (int face = 0; face < 6; ++face)
	{
		RenderFaceToViewport(deviceContext, face, cubemapSize, passName, timeSeconds);
	}

	// Deactivate render target
	m_hdrCrossTexture->deactivateRenderTarget(deviceContext);

	return true;
}

void CubemapGenerator::RenderFaceToViewport(GraphicsDeviceContext& deviceContext,
											int face, int faceSize, const std::string& passName, float timeSeconds)
{
	// Setup constants for this face
	SetupConstants(timeSeconds);
	SetupCameraConstants(face);

	// Set both constant buffers
	m_renderPlatform->SetConstantBuffer(deviceContext, &m_cubemapConstants);
	m_renderPlatform->SetConstantBuffer(deviceContext, &m_cameraConstants);

	// Calculate viewport position for this face in the cross layout
	// Cross layout:
	//     +Y
	// -X  +Z  +X  -Z
	//     -Y
	int viewportX = 0, viewportY = 0;

	switch (face)
	{
	case 0: // +X (right)
		viewportX = faceSize * 2;
		viewportY = faceSize * 1;
		break;
	case 1: // -X (left)
		viewportX = faceSize * 0;
		viewportY = faceSize * 1;
		break;
	case 2: // +Y (top)
		viewportX = faceSize * 1;
		viewportY = faceSize * 0;
		break;
	case 3: // -Y (bottom)
		viewportX = faceSize * 1;
		viewportY = faceSize * 2;
		break;
	case 4: // +Z (front)
		viewportX = faceSize * 1;
		viewportY = faceSize * 1;
		break;
	case 5: // -Z (back)
		viewportX = faceSize * 3;
		viewportY = faceSize * 1;
		break;
	}

	// Set viewport for this face
	Viewport viewport;
	viewport.x = viewportX;
	viewport.y = viewportY;
	viewport.w = faceSize;
	viewport.h = faceSize;
	m_renderPlatform->SetViewports(deviceContext, 1, &viewport);

	// Apply the unconnected shader technique and pass
	m_cubemapClearEffect->Apply(deviceContext, "unconnected", passName.c_str());

	// Draw fullscreen quad (will be clipped to viewport)
	m_renderPlatform->DrawQuad(deviceContext);

	// Unapply effect
	m_cubemapClearEffect->Unapply(deviceContext);
}



bool CubemapGenerator::SaveToHDR(platform::crossplatform::GraphicsDeviceContext &deviceContext, const std::string& filename)
{
	if (!m_hdrCrossTexture)
	{
		TELEPORT_CERR << "CubemapGenerator: No HDR cross texture to save" << std::endl;
		return false;
	}
	try
	{
		m_renderPlatform->SaveTexture(deviceContext, m_hdrCrossTexture.get(), filename.c_str());
		TELEPORT_COUT << "CubemapGenerator: Successfully saved HDR cubemap cross to " << filename << std::endl;
		return true;
	}
	catch (...)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to save HDR texture to " << filename << std::endl;
		return false;
	}
}

void CubemapGenerator::Cleanup()
{
	m_hdrCrossTexture.reset();
	m_cubemapClearEffect.reset();
	m_cubemapConstants.InvalidateDeviceObjects();
	m_cameraConstants.InvalidateDeviceObjects();
	m_initialized = false;
}
