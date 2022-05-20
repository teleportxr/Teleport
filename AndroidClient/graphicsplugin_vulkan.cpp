// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// Modifications by Simul

#include <vulkan/vulkan.hpp>
#include <array>
#include <fmt/format.h>
#include <iostream>
#include <list>
#include <map>
#include <algorithm>

#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window_jni.h>

#define XR_USE_GRAPHICS_API_VULKAN 1
#define XR_USE_PLATFORM_ANDROID 1
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "TeleportClient/OpenXR.h"
#include "AndroidOpenXR.h"
// glslangValidator doesn't wrap its output in brackets if you don't have it define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

[[noreturn]] inline void Throw(std::string failureMessage, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    if (originator != nullptr) {
        failureMessage += fmt::format("\n    Origin: {0}", originator);
    }
    if (sourceLocation != nullptr) {
        failureMessage += fmt::format("\n    Source: {0}", sourceLocation);
    }

    throw std::logic_error(failureMessage);
}

#define THROW(msg) Throw(msg, nullptr, FILE_AND_LINE);
#define CHECK(exp)                                      \
    {                                                   \
        if (!(exp)) {                                   \
            Throw("Check failed", #exp, FILE_AND_LINE); \
        }                                               \
    }
#define CHECK_MSG(exp, msg)                  \
    {                                        \
        if (!(exp)) {                        \
            Throw(msg, #exp, FILE_AND_LINE); \
        }                                    \
    }
[[noreturn]] inline void ThrowXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    Throw(fmt::format("XrResult failure [{}]", teleport::client::to_string(res)), originator, sourceLocation);
}

inline XrResult CheckXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    if (XR_FAILED(res)) {
        ThrowXrResult(res, originator, sourceLocation);
    }

    return res;
}
#define THROW_XR(xr, cmd) ThrowXrResult(xr, #cmd, FILE_AND_LINE);
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);


[[noreturn]] inline void throwVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
	throw(fmt::format("VkResult failure [%s]", teleport::android::vkResultString(res).c_str()), originator, sourceLocation);
}

inline VkResult CheckVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
	if ((res) < VK_SUCCESS) {
		throwVkResult(res, originator, sourceLocation);
	}

	return res;
}


#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
// XXX These really shouldn't have trailing ';'s
#define throw_VK(res, cmd) throwVkResult(res, #cmd, FILE_AND_LINE);
#define CHECK_VKCMD(cmd) CheckVkResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_VKRESULT(res, cmdStr) CheckVkResult(res, cmdStr, FILE_AND_LINE);


struct MemoryAllocator
{
	void Init(VkPhysicalDevice physicalDevice, VkDevice device) {
		m_vkDevice = device;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memProps);
	}

	static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	void Allocate(VkMemoryRequirements const& memReqs, VkDeviceMemory* mem, VkFlags flags = defaultFlags,
				  const void* pNext = nullptr) const {
		// Search memtypes to find first index with those properties
		for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
			if ((memReqs.memoryTypeBits & (1 << i)) != 0u) {
				// Type is available, does it match user properties?
				if ((m_memProps.memoryTypes[i].propertyFlags & flags) == flags) {
					VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, pNext};
					memAlloc.allocationSize = memReqs.size;
					memAlloc.memoryTypeIndex = i;
					CHECK_VKCMD(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, mem));
					return;
				}
			}
		}
		throw("Memory format not supported");
	}

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};
	VkPhysicalDeviceMemoryProperties m_memProps{};
};

// CmdBuffer - manage VkCommandBuffer state
struct CmdBuffer
{
#define LIST_CMDBUFFER_STATES(_) \
	_(Undefined)				 \
	_(Initialized)			   \
	_(Recording)				 \
	_(Executable)				\
	_(Executing)
	enum class CmdBufferState
	{
#define MK_ENUM(name) name,
		LIST_CMDBUFFER_STATES(MK_ENUM)
#undef MK_ENUM
	};
	CmdBufferState state{CmdBufferState::Undefined};
	VkCommandPool pool{VK_NULL_HANDLE};
	VkCommandBuffer buf{VK_NULL_HANDLE};
	VkFence execFence{VK_NULL_HANDLE};

	CmdBuffer() = default;

	CmdBuffer(const CmdBuffer&) = delete;
	CmdBuffer& operator=(const CmdBuffer&) = delete;
	CmdBuffer(CmdBuffer&&) = delete;
	CmdBuffer& operator=(CmdBuffer&&) = delete;

	~CmdBuffer() {
		SetState(CmdBufferState::Undefined);
		if (m_vkDevice != nullptr) {
			if (buf != VK_NULL_HANDLE) {
				vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
			}
			if (pool != VK_NULL_HANDLE) {
				vkDestroyCommandPool(m_vkDevice, pool, nullptr);
			}
			if (execFence != VK_NULL_HANDLE) {
				vkDestroyFence(m_vkDevice, execFence, nullptr);
			}
		}
		buf = VK_NULL_HANDLE;
		pool = VK_NULL_HANDLE;
		execFence = VK_NULL_HANDLE;
		m_vkDevice = nullptr;
	}

	std::string StateString(CmdBufferState s) {
		switch (s) {
#define MK_CASE(name)		  \
	case CmdBufferState::name: \
		return #name;
			LIST_CMDBUFFER_STATES(MK_CASE)
#undef MK_CASE
		}
		return "(Unknown)";
	}

#define CHECK_CBSTATE(s)																						   \
	do																											 \
		if (state != (s)) {																						\
			std::cerr<<(std::string("Expecting state " #s " from ") + __FUNCTION__ + ", in " + StateString(state)).c_str()<<std::endl; \
			return false;																						  \
		}																										  \
	while (0)

	bool Init(VkDevice device, uint32_t queueFamilyIndex) {
		CHECK_CBSTATE(CmdBufferState::Undefined);

		m_vkDevice = device;

		// Create a command pool to allocate our command buffer from
		VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
		CHECK_VKCMD(vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &pool));

		// Create the command buffer from the command pool
		VkCommandBufferAllocateInfo cmd{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		cmd.commandPool = pool;
		cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd.commandBufferCount = 1;
		CHECK_VKCMD(vkAllocateCommandBuffers(m_vkDevice, &cmd, &buf));

		VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		CHECK_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &execFence));

		SetState(CmdBufferState::Initialized);
		return true;
	}

	bool Begin() {
		CHECK_CBSTATE(CmdBufferState::Initialized);
		VkCommandBufferBeginInfo cmdBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		CHECK_VKCMD(vkBeginCommandBuffer(buf, &cmdBeginInfo));
		SetState(CmdBufferState::Recording);
		return true;
	}

	bool End() {
		CHECK_CBSTATE(CmdBufferState::Recording);
		CHECK_VKCMD(vkEndCommandBuffer(buf));
		SetState(CmdBufferState::Executable);
		return true;
	}

	bool Exec(VkQueue queue) {
		CHECK_CBSTATE(CmdBufferState::Executable);

		VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buf;
		CHECK_VKCMD(vkQueueSubmit(queue, 1, &submitInfo, execFence));

		SetState(CmdBufferState::Executing);
		return true;
	}

	bool Wait() {
		// Waiting on a not-in-flight command buffer is a no-op
		if (state == CmdBufferState::Initialized) {
			return true;
		}

		CHECK_CBSTATE(CmdBufferState::Executing);

		const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
		for (int i = 0; i < 5; ++i) {
			auto res = vkWaitForFences(m_vkDevice, 1, &execFence, VK_TRUE, timeoutNs);
			if (res == VK_SUCCESS) {
				// Buffer can be executed multiple times...
				SetState(CmdBufferState::Executable);
				return true;
			}
			std::cout<<"Waiting for CmdBuffer fence timed out, retrying...";
		}

		return false;
	}

	bool Reset() {
		if (state != CmdBufferState::Initialized) {
			CHECK_CBSTATE(CmdBufferState::Executable);

			CHECK_VKCMD(vkResetFences(m_vkDevice, 1, &execFence));
			CHECK_VKCMD(vkResetCommandBuffer(buf, 0));

			SetState(CmdBufferState::Initialized);
		}

		return true;
	}

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};

	void SetState(CmdBufferState newState) { state = newState; }

#undef CHECK_CBSTATE
#undef LIST_CMDBUFFER_STATES
};


// RenderPass wrapper
struct RenderPass {
	VkFormat colorFmt{};
	VkFormat depthFmt{};
	VkRenderPass pass{VK_NULL_HANDLE};

	RenderPass() = default;

	bool Create(VkDevice device, VkFormat aColorFmt, VkFormat aDepthFmt) {
		m_vkDevice = device;
		colorFmt = aColorFmt;
		depthFmt = aDepthFmt;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		std::array<VkAttachmentDescription, 2> at = {};

		VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
		rpInfo.attachmentCount = 0;
		rpInfo.pAttachments = at.data();
		rpInfo.subpassCount = 1;
		rpInfo.pSubpasses = &subpass;

		if (colorFmt != VK_FORMAT_UNDEFINED) {
			colorRef.attachment = rpInfo.attachmentCount++;

			at[colorRef.attachment].format = colorFmt;
			at[colorRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
			at[colorRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			at[colorRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			at[colorRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			at[colorRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			at[colorRef.attachment].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			at[colorRef.attachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorRef;
		}

		if (depthFmt != VK_FORMAT_UNDEFINED) {
			depthRef.attachment = rpInfo.attachmentCount++;

			at[depthRef.attachment].format = depthFmt;
			at[depthRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
			at[depthRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			at[depthRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			at[depthRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			at[depthRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			at[depthRef.attachment].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			at[depthRef.attachment].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			subpass.pDepthStencilAttachment = &depthRef;
		}

		CHECK_VKCMD(vkCreateRenderPass(m_vkDevice, &rpInfo, nullptr, &pass));

		return true;
	}

	~RenderPass() {
		if (m_vkDevice != nullptr) {
			if (pass != VK_NULL_HANDLE) {
				vkDestroyRenderPass(m_vkDevice, pass, nullptr);
			}
		}
		pass = VK_NULL_HANDLE;
		m_vkDevice = nullptr;
	}

	RenderPass(const RenderPass&) = delete;
	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass(RenderPass&&) = delete;
	RenderPass& operator=(RenderPass&&) = delete;

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};
};

// VkImage + framebuffer wrapper
struct RenderTarget {
	VkImage colorImage{VK_NULL_HANDLE};
	VkImage depthImage{VK_NULL_HANDLE};
	VkImageView colorView{VK_NULL_HANDLE};
	VkImageView depthView{VK_NULL_HANDLE};
	VkFramebuffer fb{VK_NULL_HANDLE};

	RenderTarget() = default;

	~RenderTarget() {
		if (m_vkDevice != nullptr) {
			if (fb != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(m_vkDevice, fb, nullptr);
			}
			if (colorView != VK_NULL_HANDLE) {
				vkDestroyImageView(m_vkDevice, colorView, nullptr);
			}
			if (depthView != VK_NULL_HANDLE) {
				vkDestroyImageView(m_vkDevice, depthView, nullptr);
			}
		}

		// Note we don't own color/depthImage, it will get destroyed when xrDestroySwapchain is called
		colorImage = VK_NULL_HANDLE;
		depthImage = VK_NULL_HANDLE;
		colorView = VK_NULL_HANDLE;
		depthView = VK_NULL_HANDLE;
		fb = VK_NULL_HANDLE;
		m_vkDevice = nullptr;
	}

	RenderTarget(RenderTarget&& other) noexcept : RenderTarget() {
		using std::swap;
		swap(colorImage, other.colorImage);
		swap(depthImage, other.depthImage);
		swap(colorView, other.colorView);
		swap(depthView, other.depthView);
		swap(fb, other.fb);
		swap(m_vkDevice, other.m_vkDevice);
	}
	RenderTarget& operator=(RenderTarget&& other) noexcept {
		if (&other == this) {
			return *this;
		}
		// Clean up ourselves.
		this->~RenderTarget();
		using std::swap;
		swap(colorImage, other.colorImage);
		swap(depthImage, other.depthImage);
		swap(colorView, other.colorView);
		swap(depthView, other.depthView);
		swap(fb, other.fb);
		swap(m_vkDevice, other.m_vkDevice);
		return *this;
	}
	void Create(VkDevice device, VkImage aColorImage, VkImage aDepthImage, VkExtent2D size, RenderPass& renderPass) {
		m_vkDevice = device;

		colorImage = aColorImage;
		depthImage = aDepthImage;

		std::array<VkImageView, 2> attachments{};
		uint32_t attachmentCount = 0;

		// Create color image view
		if (colorImage != VK_NULL_HANDLE) {
			VkImageViewCreateInfo colorViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
			colorViewInfo.image = colorImage;
			colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorViewInfo.format = renderPass.colorFmt;
			colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorViewInfo.subresourceRange.baseMipLevel = 0;
			colorViewInfo.subresourceRange.levelCount = 1;
			colorViewInfo.subresourceRange.baseArrayLayer = 0;
			colorViewInfo.subresourceRange.layerCount = 1;
			CHECK_VKCMD(vkCreateImageView(m_vkDevice, &colorViewInfo, nullptr, &colorView));
			attachments[attachmentCount++] = colorView;
		}

		// Create depth image view
		if (depthImage != VK_NULL_HANDLE) {
			VkImageViewCreateInfo depthViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
			depthViewInfo.image = depthImage;
			depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			depthViewInfo.format = renderPass.depthFmt;
			depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			depthViewInfo.subresourceRange.baseMipLevel = 0;
			depthViewInfo.subresourceRange.levelCount = 1;
			depthViewInfo.subresourceRange.baseArrayLayer = 0;
			depthViewInfo.subresourceRange.layerCount = 1;
			CHECK_VKCMD(vkCreateImageView(m_vkDevice, &depthViewInfo, nullptr, &depthView));
			attachments[attachmentCount++] = depthView;
		}

		VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		fbInfo.renderPass = renderPass.pass;
		fbInfo.attachmentCount = attachmentCount;
		fbInfo.pAttachments = attachments.data();
		fbInfo.width = size.width;
		fbInfo.height = size.height;
		fbInfo.layers = 1;
		CHECK_VKCMD(vkCreateFramebuffer(m_vkDevice, &fbInfo, nullptr, &fb));
	}

	RenderTarget(const RenderTarget&) = delete;
	RenderTarget& operator=(const RenderTarget&) = delete;

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};
};

// Simple vertex MVP xform & color fragment shader layout
struct PipelineLayout {
	VkPipelineLayout layout{VK_NULL_HANDLE};

	PipelineLayout() = default;

	~PipelineLayout() {
		if (m_vkDevice != nullptr) {
			if (layout != VK_NULL_HANDLE) {
				vkDestroyPipelineLayout(m_vkDevice, layout, nullptr);
			}
		}
		layout = VK_NULL_HANDLE;
		m_vkDevice = nullptr;
	}

	void Create(VkDevice device) {
		m_vkDevice = device;

		// MVP matrix is a push_constant
		VkPushConstantRange pcr = {};
		pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pcr.offset = 0;
		pcr.size = 4 * 4 * sizeof(float);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pcr;
		CHECK_VKCMD(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &layout));
	}

	PipelineLayout(const PipelineLayout&) = delete;
	PipelineLayout& operator=(const PipelineLayout&) = delete;
	PipelineLayout(PipelineLayout&&) = delete;
	PipelineLayout& operator=(PipelineLayout&&) = delete;

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};
};


struct DepthBuffer {
	VkDeviceMemory depthMemory{VK_NULL_HANDLE};
	VkImage depthImage{VK_NULL_HANDLE};

	DepthBuffer() = default;

	~DepthBuffer() {
		if (m_vkDevice != nullptr) {
			if (depthImage != VK_NULL_HANDLE) {
				vkDestroyImage(m_vkDevice, depthImage, nullptr);
			}
			if (depthMemory != VK_NULL_HANDLE) {
				vkFreeMemory(m_vkDevice, depthMemory, nullptr);
			}
		}
		depthImage = VK_NULL_HANDLE;
		depthMemory = VK_NULL_HANDLE;
		m_vkDevice = nullptr;
	}

	DepthBuffer(DepthBuffer&& other) noexcept : DepthBuffer() {
		using std::swap;

		swap(depthImage, other.depthImage);
		swap(depthMemory, other.depthMemory);
		swap(m_vkDevice, other.m_vkDevice);
	}
	DepthBuffer& operator=(DepthBuffer&& other) noexcept {
		if (&other == this) {
			return *this;
		}
		// clean up self
		this->~DepthBuffer();
		using std::swap;

		swap(depthImage, other.depthImage);
		swap(depthMemory, other.depthMemory);
		swap(m_vkDevice, other.m_vkDevice);
		return *this;
	}

	void Create(VkDevice device, MemoryAllocator* memAllocator, VkFormat depthFormat,
				const XrSwapchainCreateInfo& swapchainCreateInfo) {
		m_vkDevice = device;

		VkExtent2D size = {swapchainCreateInfo.width, swapchainCreateInfo.height};

		// Create a D32 depthbuffer
		VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = size.width;
		imageInfo.extent.height = size.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = depthFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.samples = (VkSampleCountFlagBits)swapchainCreateInfo.sampleCount;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CHECK_VKCMD(vkCreateImage(device, &imageInfo, nullptr, &depthImage));

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
		memAllocator->Allocate(memRequirements, &depthMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CHECK_VKCMD(vkBindImageMemory(device, depthImage, depthMemory, 0));
	}

	void TransitionLayout(CmdBuffer* cmdBuffer, VkImageLayout newLayout) {
		if (newLayout == m_vkLayout) {
			return;
		}

		VkImageMemoryBarrier depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		depthBarrier.oldLayout = m_vkLayout;
		depthBarrier.newLayout = newLayout;
		depthBarrier.image = depthImage;
		depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
		vkCmdPipelineBarrier(cmdBuffer->buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr,
							 0, nullptr, 1, &depthBarrier);

		m_vkLayout = newLayout;
	}

	DepthBuffer(const DepthBuffer&) = delete;
	DepthBuffer& operator=(const DepthBuffer&) = delete;

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};
	VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct SwapchainImageContext
{
	SwapchainImageContext(XrStructureType _swapchainImageType) : swapchainImageType(_swapchainImageType) {}

	// A packed array of XrSwapchainImageVulkan2KHR's for xrEnumerateSwapchainImages
	std::vector<XrSwapchainImageVulkan2KHR> swapchainImages;
	std::vector<RenderTarget> renderTarget;
	VkExtent2D size{};
	DepthBuffer depthBuffer{};
	RenderPass rp{};
	//Pipeline pipe{};
	XrStructureType swapchainImageType;

	SwapchainImageContext() = default;

	std::vector<XrSwapchainImageBaseHeader*> Create(VkDevice device, MemoryAllocator* memAllocator, uint32_t capacity,
													const XrSwapchainCreateInfo& swapchainCreateInfo, const PipelineLayout& layout) {
		m_vkDevice = device;

		size = {swapchainCreateInfo.width, swapchainCreateInfo.height};
		VkFormat colorFormat = (VkFormat)swapchainCreateInfo.format;
		VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
		// XXX handle swapchainCreateInfo.sampleCount

		depthBuffer.Create(m_vkDevice, memAllocator, depthFormat, swapchainCreateInfo);
		rp.Create(m_vkDevice, colorFormat, depthFormat);

		swapchainImages.resize(capacity);
		renderTarget.resize(capacity);
		std::vector<XrSwapchainImageBaseHeader*> bases(capacity);
		for (uint32_t i = 0; i < capacity; ++i) {
			swapchainImages[i] = {swapchainImageType};
			bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&swapchainImages[i]);
		}

		return bases;
	}

	uint32_t ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader) {
		auto p = reinterpret_cast<const XrSwapchainImageVulkan2KHR*>(swapchainImageHeader);
		return (uint32_t)(p - &swapchainImages[0]);
	}

	void BindRenderTarget(uint32_t index, VkRenderPassBeginInfo* renderPassBeginInfo) {
		if (renderTarget[index].fb == VK_NULL_HANDLE) {
			renderTarget[index].Create(m_vkDevice, swapchainImages[index].image, depthBuffer.depthImage, size, rp);
		}
		renderPassBeginInfo->renderPass = rp.pass;
		renderPassBeginInfo->framebuffer = renderTarget[index].fb;
		renderPassBeginInfo->renderArea.offset = {0, 0};
		renderPassBeginInfo->renderArea.extent = size;
	}

   private:
	VkDevice m_vkDevice{VK_NULL_HANDLE};
};

struct VulkanGraphicsPlugin
{
	VulkanGraphicsPlugin()
	{
		m_graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
	}

	std::vector<std::string> GetInstanceExtensions() const  { return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME}; }

	// Note: The output must not outlive the input - this modifies the input and returns a collection of views into that modified
	// input!
	std::vector<const char*> ParseExtensionString(char* names)
	{
		std::vector<const char*> list;
		while (*names != 0)
		{
			list.push_back(names);
			while (*(++names) != 0)
			{
				if (*names == ' ')
				{
					*names++ = '\0';
					break;
				}
			}
		}
		return list;
	}

	const char* GetValidationLayerName()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		std::vector<const char*> validationLayerNames;
		validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");
		validationLayerNames.push_back("VK_LAYER_LUNARG_standard_validation");

		// Enable only one validation layer from the list above. Prefer KHRONOS.
		for (auto& validationLayerName : validationLayerNames)
		{
			for (const auto& layerProperties : availableLayers)
			{
				if (0 == strcmp(validationLayerName, layerProperties.layerName))
				{
					return validationLayerName;
				}
			}
		}

		return nullptr;
	}


	const XrBaseInStructure* GetGraphicsBinding() const  {
		return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
	}

	std::vector<XrSwapchainImageBaseHeader*> AllocateSwapchainImageStructs(
		uint32_t capacity, const XrSwapchainCreateInfo& swapchainCreateInfo)  {
		// Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
		// Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
		// Keep the buffer alive by adding it into the list of buffers.
		m_swapchainImageContexts.emplace_back(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR);
		SwapchainImageContext& swapchainImageContext = m_swapchainImageContexts.back();

		std::vector<XrSwapchainImageBaseHeader*> bases = swapchainImageContext.Create(
			m_vkDevice, &m_memAllocator, capacity, swapchainCreateInfo, m_pipelineLayout);

		// Map every swapchainImage base pointer to this context
		for (auto& base : bases) {
			m_swapchainImageContextMap[base] = &swapchainImageContext;
		}

		return bases;
	}

	void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
			int64_t /*swapchainFormat*/)
	{
		CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.

		auto swapchainContext = m_swapchainImageContextMap[swapchainImage];
		uint32_t imageIndex = swapchainContext->ImageIndex(swapchainImage);

		m_cmdBuffer.Reset();
		m_cmdBuffer.Begin();

		// Ensure depth is in the right layout
		swapchainContext->depthBuffer.TransitionLayout(&m_cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);


		m_cmdBuffer.End();
		m_cmdBuffer.Exec(m_vkQueue);
		// XXX Should double-buffer the command buffers, for now just flush
		m_cmdBuffer.Wait();

	}

	uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView&)  { return VK_SAMPLE_COUNT_1_BIT; }

protected:
	XrGraphicsBindingVulkan2KHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
	std::list<SwapchainImageContext> m_swapchainImageContexts;
	std::map<const XrSwapchainImageBaseHeader*, SwapchainImageContext*> m_swapchainImageContextMap;

	VkInstance m_vkInstance{VK_NULL_HANDLE};
	VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
	VkDevice m_vkDevice{VK_NULL_HANDLE};
	uint32_t m_queueFamilyIndex = 0;
	VkQueue m_vkQueue{VK_NULL_HANDLE};
	VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

	MemoryAllocator m_memAllocator{};
	CmdBuffer m_cmdBuffer{};
	PipelineLayout m_pipelineLayout{};
};
