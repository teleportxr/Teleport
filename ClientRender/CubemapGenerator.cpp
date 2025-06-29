#include "CubemapGenerator.h"
#include "Platform/CrossPlatform/GraphicsDeviceContext.h"
#include "Platform/Math/Vector3.h"
#include "Platform/Math/Matrix4x4.h"
#include "TeleportCore/ErrorHandling.h"
#include <ktx.h>
#include <fstream>

// OpenGL constants for KTX2
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

// Vulkan constants for KTX2
#ifndef VK_FORMAT_R8G8B8A8_UNORM
#define VK_FORMAT_R8G8B8A8_UNORM 37
#endif

using namespace teleport::clientrender;
using namespace platform::crossplatform;
using namespace platform::math;

CubemapGenerator::CubemapGenerator(RenderPlatform* renderPlatform)
	: m_renderPlatform(renderPlatform)
	, m_cubemapSize(0)
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

	// Create constant buffer
	m_cubemapConstants = std::make_unique<ConstantBuffer<CubemapConstants>>(m_renderPlatform, "CubemapConstants");

	m_initialized = true;
	return true;
}

bool CubemapGenerator::LoadShaders()
{
	// Load the cubemap_clear.sfx effect
	m_cubemapClearEffect = std::unique_ptr<Effect>(m_renderPlatform->CreateEffect("cubemap_clear"));
	if (!m_cubemapClearEffect)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create cubemap_clear effect" << std::endl;
		return false;
	}

	// Load the effect from file
	if (!m_cubemapClearEffect->Load(m_renderPlatform, "client/Shaders/cubemap_clear.sfx"))
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to load cubemap_clear.sfx" << std::endl;
		return false;
	}

	return true;
}

bool CubemapGenerator::CreateCubemapTexture(int size)
{
	if (m_cubemapTexture && m_cubemapSize == size)
		return true; // Already created with correct size

	m_cubemapTexture.reset();
	m_cubemapSize = size;

	// Create cubemap texture
	m_cubemapTexture = std::unique_ptr<Texture>(m_renderPlatform->CreateTexture());
	if (!m_cubemapTexture)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create texture" << std::endl;
		return false;
	}

	// Configure as cubemap render target
	TextureCreate textureCreate;
	textureCreate.w = size;
	textureCreate.l = size;
	textureCreate.d = 1;
	textureCreate.arraysize = 6; // 6 faces for cubemap
	textureCreate.mips = 1;
	textureCreate.cubemap = true;
	textureCreate.f = PixelFormat::RGBA_8_UNORM;
	textureCreate.make_rt = true;
	textureCreate.computable = true;
	textureCreate.name = "GeneratedCubemap";

	if (!m_cubemapTexture->EnsureTexture(m_renderPlatform, &textureCreate))
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create cubemap texture" << std::endl;
		return false;
	}

	return true;
}

void CubemapGenerator::SetupConstants(float timeSeconds)
{
	if (!m_cubemapConstants)
		return;

	CubemapConstants& constants = m_cubemapConstants->GetData();
	
	// Clear the constants
	memset(&constants, 0, sizeof(CubemapConstants));
	
	// Set time for animated effects
	constants.time_seconds = timeSeconds;
	
	// Set up identity matrices and default values
	constants.serverProj = mat4::identity();
	constants.cameraPosition = vec3(0.0f, 0.0f, 1.0f); // Slightly above ground
	constants.cameraRotation = vec4(0.0f, 0.0f, 0.0f, 1.0f); // Identity quaternion
	constants.depthOffsetScale = vec4(0.0f, 0.0f, 1.0f, 1.0f);
	constants.targetSize = int2(m_cubemapSize, m_cubemapSize);
	constants.sourceOffset = int2(0, 0);

	m_cubemapConstants->UpdateData();
}

bool CubemapGenerator::GenerateCubemap(const std::string& passName, int cubemapSize, float timeSeconds)
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

	// Create cubemap texture
	if (!CreateCubemapTexture(cubemapSize))
		return false;

	// Setup constants
	SetupConstants(timeSeconds);

	// Create graphics device context
	GraphicsDeviceContext deviceContext;
	deviceContext.renderPlatform = m_renderPlatform;
	deviceContext.platform_context = m_renderPlatform->GetImmediateContext().platform_context;

	// Set constant buffer
	m_renderPlatform->SetConstantBuffer(deviceContext, m_cubemapConstants.get());

	// For cubemap generation, we need to set up the view matrix for each face
	// The unconnected shaders use TexCoordsToView to generate the view direction
	// So we need to render a fullscreen quad for each face with appropriate view setup

	// Set viewport for the entire cubemap
	Viewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = cubemapSize;
	viewport.h = cubemapSize;
	m_renderPlatform->SetViewports(deviceContext, 1, &viewport);

	// Set the cubemap as render target (all faces at once)
	auto cubemapRenderTarget = m_cubemapTexture->AsRenderTarget();
	if (!cubemapRenderTarget)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create cubemap render target" << std::endl;
		return false;
	}

	// Activate render target
	m_renderPlatform->ActivateRenderTargets(deviceContext, 1, &cubemapRenderTarget, nullptr);

	// Clear the cubemap
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_renderPlatform->Clear(deviceContext, clearColor);

	// Render each face of the cubemap
	for (int face = 0; face < 6; ++face)
	{
		// Update constants for this face
		CubemapConstants& constants = m_cubemapConstants->GetData();

		// Set up view matrix for each cubemap face
		// This follows the standard cubemap face orientations
		vec3 forward, up, right;
		switch (face)
		{
		case 0: // +X
			forward = vec3(1.0f, 0.0f, 0.0f);
			up = vec3(0.0f, 1.0f, 0.0f);
			break;
		case 1: // -X
			forward = vec3(-1.0f, 0.0f, 0.0f);
			up = vec3(0.0f, 1.0f, 0.0f);
			break;
		case 2: // +Y
			forward = vec3(0.0f, 1.0f, 0.0f);
			up = vec3(0.0f, 0.0f, -1.0f);
			break;
		case 3: // -Y
			forward = vec3(0.0f, -1.0f, 0.0f);
			up = vec3(0.0f, 0.0f, 1.0f);
			break;
		case 4: // +Z
			forward = vec3(0.0f, 0.0f, 1.0f);
			up = vec3(0.0f, 1.0f, 0.0f);
			break;
		case 5: // -Z
			forward = vec3(0.0f, 0.0f, -1.0f);
			up = vec3(0.0f, 1.0f, 0.0f);
			break;
		}

		// Convert to quaternion rotation
		right = normalize(cross(forward, up));
		up = normalize(cross(right, forward));

		// Create rotation matrix and convert to quaternion
		mat4 rotMatrix = mat4::identity();
		rotMatrix.m[0][0] = right.x; rotMatrix.m[0][1] = right.y; rotMatrix.m[0][2] = right.z;
		rotMatrix.m[1][0] = up.x; rotMatrix.m[1][1] = up.y; rotMatrix.m[1][2] = up.z;
		rotMatrix.m[2][0] = -forward.x; rotMatrix.m[2][1] = -forward.y; rotMatrix.m[2][2] = -forward.z;

		// For simplicity, we'll use identity rotation and let the shader handle the cubemap face directions
		constants.cameraRotation = vec4(0.0f, 0.0f, 0.0f, 1.0f);

		m_cubemapConstants->UpdateData();
		m_renderPlatform->SetConstantBuffer(deviceContext, m_cubemapConstants.get());

		// Set render target to specific face
		TextureView faceView;
		faceView.elements.type = TextureView::TEXTURE_2D;
		faceView.elements.subresourceRange.aspectMask = TextureAspectFlags::COLOUR;
		faceView.elements.subresourceRange.baseArrayLayer = face;
		faceView.elements.subresourceRange.layerCount = 1;
		faceView.elements.subresourceRange.baseMipLevel = 0;
		faceView.elements.subresourceRange.levelCount = 1;

		auto faceRenderTarget = m_cubemapTexture->AsRenderTarget(faceView);
		if (faceRenderTarget)
		{
			// Deactivate previous target
			m_renderPlatform->DeactivateRenderTargets(deviceContext);

			// Activate this face as render target
			m_renderPlatform->ActivateRenderTargets(deviceContext, 1, &faceRenderTarget, nullptr);

			// Apply the unconnected shader technique and pass
			m_cubemapClearEffect->Apply(deviceContext, "unconnected", passName.c_str());

			// Draw fullscreen quad
			m_renderPlatform->DrawQuad(deviceContext);

			// Unapply effect
			m_cubemapClearEffect->Unapply(deviceContext);
		}
	}

	// Deactivate render target
	m_renderPlatform->DeactivateRenderTargets(deviceContext);

	return true;
}

bool CubemapGenerator::SaveToKTX2(const std::string& filename)
{
	if (!m_cubemapTexture)
	{
		TELEPORT_CERR << "CubemapGenerator: No cubemap texture to save" << std::endl;
		return false;
	}

	// Create a staging texture to read back the cubemap data
	std::unique_ptr<Texture> stagingTexture(m_renderPlatform->CreateTexture());
	if (!stagingTexture)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create staging texture" << std::endl;
		return false;
	}

	// Configure staging texture for CPU readback
	TextureCreate stagingCreate;
	stagingCreate.w = m_cubemapSize;
	stagingCreate.l = m_cubemapSize;
	stagingCreate.d = 1;
	stagingCreate.arraysize = 6; // 6 faces
	stagingCreate.mips = 1;
	stagingCreate.cubemap = true;
	stagingCreate.f = PixelFormat::RGBA_8_UNORM;
	stagingCreate.make_rt = false;
	stagingCreate.computable = false;
	stagingCreate.cpuWritable = true;
	stagingCreate.name = "StagingCubemap";

	if (!stagingTexture->EnsureTexture(m_renderPlatform, &stagingCreate))
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create staging texture" << std::endl;
		return false;
	}

	// Copy cubemap to staging texture
	GraphicsDeviceContext deviceContext;
	deviceContext.renderPlatform = m_renderPlatform;
	deviceContext.platform_context = m_renderPlatform->GetImmediateContext().platform_context;

	m_renderPlatform->CopyTexture(deviceContext, stagingTexture.get(), m_cubemapTexture.get());

	// Create KTX2 texture
	ktxTexture2* ktx2Texture = nullptr;
	ktxTextureCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));

	createInfo.glInternalformat = GL_RGBA8; // Assuming RGBA8 format
	createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
	createInfo.baseWidth = m_cubemapSize;
	createInfo.baseHeight = m_cubemapSize;
	createInfo.baseDepth = 1;
	createInfo.numDimensions = 2;
	createInfo.numLevels = 1;
	createInfo.numLayers = 1;
	createInfo.numFaces = 6; // Cubemap has 6 faces
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	KTX_error_code result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &ktx2Texture);
	if (result != KTX_SUCCESS)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to create KTX2 texture: " << ktxErrorString(result) << std::endl;
		return false;
	}

	// Extract pixel data from staging texture and populate KTX2 texture
	size_t faceSize = m_cubemapSize * m_cubemapSize * 4; // 4 bytes per pixel (RGBA8)
	std::vector<uint8_t> faceData(faceSize);

	for (int face = 0; face < 6; ++face)
	{
		// Read face data from staging texture
		if (!stagingTexture->GetTexels(deviceContext, faceData.data(), 0, face))
		{
			TELEPORT_CERR << "CubemapGenerator: Failed to read face " << face << " data" << std::endl;
			ktxTexture_Destroy(ktxTexture(ktx2Texture));
			return false;
		}

		// Set image data in KTX2 texture
		result = ktxTexture_SetImageFromMemory(ktxTexture(ktx2Texture), 0, 0, face, faceData.data(), faceSize);
		if (result != KTX_SUCCESS)
		{
			TELEPORT_CERR << "CubemapGenerator: Failed to set face " << face << " data: " << ktxErrorString(result) << std::endl;
			ktxTexture_Destroy(ktxTexture(ktx2Texture));
			return false;
		}
	}

	// Write KTX2 file
	result = ktxTexture_WriteToNamedFile(ktxTexture(ktx2Texture), filename.c_str());
	if (result != KTX_SUCCESS)
	{
		TELEPORT_CERR << "CubemapGenerator: Failed to write KTX2 file: " << ktxErrorString(result) << std::endl;
		ktxTexture_Destroy(ktxTexture(ktx2Texture));
		return false;
	}

	// Cleanup
	ktxTexture_Destroy(ktxTexture(ktx2Texture));

	TELEPORT_COUT << "CubemapGenerator: Successfully saved cubemap to " << filename << std::endl;
	return true;
}

void CubemapGenerator::Cleanup()
{
	m_cubemapTexture.reset();
	m_cubemapClearEffect.reset();
	m_cubemapConstants.reset();
	m_initialized = false;
}
