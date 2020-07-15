// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include <libavstream/common.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

struct GLFWwindow;
struct ImGuiContext;

namespace avs::test {

struct Texture
{
	virtual ~Texture() = default;
	virtual SurfaceBackendInterface* createSurface() const = 0;
};
using TextureHandle = std::shared_ptr<Texture>;

struct Shader
{
	virtual ~Shader() = default;
};
using ShaderHandle = std::shared_ptr<Shader>;

struct RendererStats
{
	uint64_t frameCounter;
	double lastFrameTime;
	double lastFPS;
};

class RendererInterface
{
public:
	virtual ~RendererInterface() = default;

	virtual GLFWwindow* initialize(int frameWidth, int frameHeight) = 0;
	virtual TextureHandle createTexture(int width, int height, SurfaceFormat format) = 0;
	virtual ShaderHandle createShader(const std::string& filename) = 0;

	virtual void render(ShaderHandle shaderHandle, TextureHandle textureHandle, uint32_t id, float time) = 0;
	virtual void display(const std::vector<TextureHandle>& textureHandles) = 0;

	virtual void beginFrame() = 0;
	virtual void endFrame(bool vsync) = 0;

	virtual DeviceHandle getDevice() const = 0;
	virtual DeviceHandle getContext() const = 0;

	virtual RendererStats getStats() const = 0;
};

} // avs::test