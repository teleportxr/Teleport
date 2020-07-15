// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <stdexcept>
#include <util.hpp>
#include <renderer_dx11.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <d3dcompiler.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx11.h>

namespace avs::test {

static const char* presentShaders = R"(
	Texture2D texture0 : register(t0);
	Texture2D texture1 : register(t1);
	SamplerState defaultSampler : register(s0);
	
	struct PixelShaderInput
	{
		float4 position : SV_POSITION;
		float2 texcoord : TEXCOORD;
	};

	PixelShaderInput main_vs(uint vertexID : SV_VertexID)
	{
		PixelShaderInput vout;

		if(vertexID == 0) {
			vout.texcoord = float2(1.0, -1.0);
			vout.position = float4(1.0, 3.0, 0.0, 1.0);
		}
		else if(vertexID == 1) {
			vout.texcoord = float2(-1.0, 1.0);
			vout.position = float4(-3.0, -1.0, 0.0, 1.0);
		}
		else /* if(vertexID == 2) */ {
			vout.texcoord = float2(1.0, 1.0);
			vout.position = float4(1.0, -1.0, 0.0, 1.0);
		}
		return vout;
	}

	float4 main_ps(PixelShaderInput pin) : SV_Target
	{
		return pin.texcoord.x < 0.5 ? 
			texture0.Sample(defaultSampler, pin.texcoord):
			texture1.Sample(defaultSampler, pin.texcoord);
	}
)";

struct RenderConstants
{
	uint32_t id;
	float time;
	float _padding[2];
};
	
RendererDX11::~RendererDX11()
{
	if(m_imguiContext) {
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}

GLFWwindow* RendererDX11::initialize(int frameWidth, int frameHeight)
{
	if(glfwInit() != GLFW_TRUE) {
		throw std::runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(frameWidth, frameHeight, "Test Renderer (DX11)", nullptr, nullptr);
	if(!window) {
		throw std::runtime_error("Failed to create GLFW window");
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width  = frameWidth;
	swapChainDesc.BufferDesc.Height = frameHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.OutputWindow = glfwGetWin32Window(window);
	swapChainDesc.Windowed = true;

	UINT deviceFlags = 0;
#if _DEBUG
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc, &m_swapChain, &m_device, nullptr, &m_context))) {
		throw std::runtime_error("Failed to create D3D11 device and swap chain");
	}

	{
		ComPtr<ID3D11Texture2D> backBuffer;
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		if(FAILED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_backBufferView))) {
			throw std::runtime_error("Failed to create back buffer render target view");
		}
	}

	D3D11_BUFFER_DESC renderConstantsDesc = {};
	renderConstantsDesc.ByteWidth = sizeof(RenderConstants);
	renderConstantsDesc.Usage = D3D11_USAGE_DEFAULT;
	renderConstantsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if(FAILED(m_device->CreateBuffer(&renderConstantsDesc, nullptr, &m_renderCB))) {
		throw std::runtime_error("Failed to create render constant buffer");
	}

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	if(FAILED(m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState))) {
		throw std::runtime_error("Failed to create default rasterizer state");
	}

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxAnisotropy = 1;
	if(FAILED(m_device->CreateSamplerState(&samplerDesc, &m_samplerState))) {
		throw std::runtime_error("Failed to create default sampler state");
	}
	
	D3D11_VIEWPORT viewport = {};
	viewport.Width  = (FLOAT)frameWidth;
	viewport.Height = (FLOAT)frameHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_context->RSSetViewports(1, &viewport);

	{
		ComPtr<ID3DBlob> vsBytecode = compileShader(presentShaders, "main_vs", "vs_5_0");
		ComPtr<ID3DBlob> psBytecode = compileShader(presentShaders, "main_ps", "ps_5_0");

		if(FAILED(m_device->CreateVertexShader(vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(), nullptr, &m_presentVS))) {
			throw std::runtime_error("Failed to create presentation vertex shader");
		}
		if(FAILED(m_device->CreatePixelShader(psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(), nullptr, &m_presentPS))) {
			throw std::runtime_error("Failed to create presentation pixel shader");
		}
	}

	IMGUI_CHECKVERSION();
	m_imguiContext = ImGui::CreateContext();
	ImGui_ImplGlfw_InitForDirectX(window, true);
	ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

	return window;
}
	
TextureHandle RendererDX11::createTexture(int width, int height, SurfaceFormat format)
{
	DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
	switch(format) {
	case SurfaceFormat::ARGB:
		dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case SurfaceFormat::ABGR:
		dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case SurfaceFormat::NV12:
		dxgiFormat = DXGI_FORMAT_NV12;
		break;
	case SurfaceFormat::R16:
		dxgiFormat = DXGI_FORMAT_R16_FLOAT;
		break;
	default:
		assert(false);
	}

	std::shared_ptr<TextureDX11> texture(new TextureDX11);

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width  = width;
	desc.Height = height;
	desc.Format = dxgiFormat;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	if(FAILED(m_device->CreateTexture2D(&desc, nullptr, &texture->texture))) {
		throw std::runtime_error("Failed to create 2D texture");
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	if(FAILED(m_device->CreateShaderResourceView(texture->texture.Get(), &srvDesc, &texture->srv))) {
		throw std::runtime_error("Failed to create 2D texture shader resource view");
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = desc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	if(FAILED(m_device->CreateUnorderedAccessView(texture->texture.Get(), &uavDesc, &texture->uav))) {
		throw std::runtime_error("Failed to create 2D texture unordered access view");
	}

	return texture;
}
	
ShaderHandle RendererDX11::createShader(const std::string& filename)
{
	std::shared_ptr<ShaderDX11> shader(new ShaderDX11);

	ComPtr<ID3DBlob> csBytecode = compileShaderFromFile(filename.c_str(), "main", "cs_5_0");
	if(FAILED(m_device->CreateComputeShader(csBytecode->GetBufferPointer(), csBytecode->GetBufferSize(), nullptr, &shader->shader))) {
		throw std::runtime_error("Failed to create test renderer compute shader");
	}

	return shader;
}
	
void RendererDX11::render(ShaderHandle shaderHandle, TextureHandle textureHandle, uint32_t id, float time)
{
	static const UINT GroupDim = 8;
	static ID3D11UnorderedAccessView* const nullUAV[] = { nullptr };

	const ShaderDX11* shader = dynamic_cast<const ShaderDX11*>(shaderHandle.get());
	if(!shader || !shader->shader) {
		throw std::runtime_error("Invalid render shader handle");
	}

	const TextureDX11* texture = dynamic_cast<const TextureDX11*>(textureHandle.get());
	if(!texture || !texture->texture) {
		throw std::runtime_error("Invalid render texture handle");
	}

	{
		RenderConstants renderConstants;
		renderConstants.id = id;
		renderConstants.time = time;
		m_context->UpdateSubresource(m_renderCB.Get(), 0, nullptr, &renderConstants, 0, 0);
	}

	D3D11_TEXTURE2D_DESC textureDesc;
	texture->texture->GetDesc(&textureDesc);

	m_context->CSSetShader(shader->shader.Get(), nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, m_renderCB.GetAddressOf());

	m_context->CSSetUnorderedAccessViews(0, 1, texture->uav.GetAddressOf(), nullptr);
	m_context->Dispatch(textureDesc.Width / GroupDim, textureDesc.Height / GroupDim, 1);
	m_context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
}

void RendererDX11::display(const std::vector<TextureHandle>& textureHandles)
{
	static ID3D11ShaderResourceView* const nullSRV[] = { nullptr };

	std::vector<ID3D11ShaderResourceView*> textureSRVs;
	for(const auto& handle : textureHandles) {
		const TextureDX11* texture = dynamic_cast<const TextureDX11*>(handle.get());
		if(!texture || !texture->texture) {
			throw std::runtime_error("Invalid presentation texture handle");
		}
		textureSRVs.push_back(texture->srv.Get());
	}

	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->IASetInputLayout(nullptr);

	m_context->RSSetState(m_rasterizerState.Get());
	m_context->VSSetShader(m_presentVS.Get(), nullptr, 0);
	m_context->PSSetShader(m_presentPS.Get(), nullptr, 0);
	m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	
	m_context->PSSetShaderResources(0, textureSRVs.size(), textureSRVs.data());
	m_context->Draw(3, 0);
	m_context->PSSetShaderResources(0, 1, nullSRV);
}

void RendererDX11::beginFrame()
{
	m_context->OMSetRenderTargets(1, m_backBufferView.GetAddressOf(), nullptr);

	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();
}

void RendererDX11::endFrame(bool vsync)
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	m_swapChain->Present(vsync ? 1 : 0, 0);

	const double thisFrameTimestamp = glfwGetTime();
	if(m_lastFrameTimestamp > 0.0) {
		const double dT = thisFrameTimestamp - m_lastFrameTimestamp;
		m_stats.lastFrameTime = dT;
		m_stats.lastFPS = 1.0 / dT;
		++m_stats.frameCounter;
	}
	m_lastFrameTimestamp = thisFrameTimestamp;
}
	
ComPtr<ID3DBlob> RendererDX11::compileShader(const char* source, const char* entryPoint, const char* profile)
{
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;
	if(FAILED(D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, entryPoint, profile, flags, 0, &shaderBlob, &errorBlob))) {
		std::string errorMsg = "Unknown error";
		if(errorBlob) {
			errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
		}
		throw std::runtime_error(std::string{ "Failed to compile shader:\n" } + errorMsg);
	}
	return shaderBlob;
}
	
ComPtr<ID3DBlob> RendererDX11::compileShaderFromFile(const char* filename, const char* entryPoint, const char* profile)
{
	const std::string source = Utility::readTextFile(filename);
	return compileShader(source.c_str(), entryPoint, profile);
}

} // avs::test