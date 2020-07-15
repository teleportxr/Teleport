// libavstream
// (c) Copyright 2018 Simul.co

#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <wrl/client.h>

#include <libavstream/surfaces/surface_dx11.hpp>
#include <renderer_interface.hpp>

namespace avs::test {

using Microsoft::WRL::ComPtr;

struct TextureDX11 final : public Texture
{
	SurfaceBackendInterface* createSurface() const override
	{
		return new SurfaceDX11(texture.Get());
	}

	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> srv;
	ComPtr<ID3D11UnorderedAccessView> uav;
};

struct ShaderDX11 final : public Shader
{
	ComPtr<ID3D11ComputeShader> shader;
};

class RendererDX11 final : public RendererInterface
{
public:
	~RendererDX11();

	GLFWwindow* initialize(int frameWidth, int frameHeight) override;
	TextureHandle createTexture(int width, int height, SurfaceFormat format) override;
	ShaderHandle createShader(const std::string& filename) override;
	
	void render(ShaderHandle shaderHandle, TextureHandle textureHandle, uint32_t id, float time) override;
	void display(const std::vector<TextureHandle>& textureHandles) override;

	void beginFrame() override;
	void endFrame(bool vsync) override;

	DeviceHandle getDevice() const override
	{
		return DeviceHandle{ DeviceType::Direct3D11, reinterpret_cast<void*>(m_device.Get()) };
	}
	DeviceHandle getContext() const override
	{
		return DeviceHandle{ DeviceType::Direct3D11, reinterpret_cast<void*>(m_context.Get()) };
	}
	RendererStats getStats() const override
	{
		return m_stats;
	}

private:
	static ComPtr<ID3DBlob> compileShader(const char* source, const char* entryPoint, const char* profile);
	static ComPtr<ID3DBlob> compileShaderFromFile(const char* filename, const char* entryPoint, const char* profile);

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	ComPtr<IDXGISwapChain> m_swapChain;
	ComPtr<ID3D11RenderTargetView> m_backBufferView;

	ComPtr<ID3D11RasterizerState> m_rasterizerState;
	ComPtr<ID3D11SamplerState> m_samplerState;
	ComPtr<ID3D11VertexShader> m_presentVS;
	ComPtr<ID3D11PixelShader> m_presentPS;

	ComPtr<ID3D11Buffer> m_renderCB;

	double m_lastFrameTimestamp = -1.0;
	RendererStats m_stats = {0};

	ImGuiContext* m_imguiContext = nullptr;
};

} // avs::test