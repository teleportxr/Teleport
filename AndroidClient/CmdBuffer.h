#pragma once
#include "redist/android/native_app_glue/android_native_app_glue.h"
#include <vulkan/vulkan.hpp>
namespace teleport
{
	namespace android
	{
		// CmdBuffer - manage VkCommandBuffer state
		struct CmdBuffer
		{
			enum class CmdBufferState
			{
				Undefined,
				Initialized,
				Recording,
				Executable,
				Executing
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

			~CmdBuffer();

			std::string StateString(CmdBufferState s);

			bool Init(VkDevice device, uint32_t queueFamilyIndex);
			bool Begin();
			bool End();
			bool Exec(VkQueue queue);
			bool Wait();
			bool Reset();
		   private:
			VkDevice m_vkDevice{VK_NULL_HANDLE};

			void SetState(CmdBufferState newState) { state = newState; }
		};
	}
}