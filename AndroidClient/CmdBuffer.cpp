#include "CmdBuffer.h"
#include "AndroidOpenXR.h"
#include <iostream>
#include <fmt/format.h>
using namespace teleport;
using namespace android;

#define CHECK_CBSTATE(s)																						   \
	do																											 \
		if (state != (s)) {																						\
			std::cerr<<(std::string("Expecting state " #s " from ") + __FUNCTION__ + ", in " + StateString(state)).c_str()<<std::endl; \
			return false;																						  \
		}																										  \
	while (0)

#define THROW_XR(xr, cmd) ThrowXrResult(xr, #cmd, FILE_AND_LINE);
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);


[[noreturn]] void throwVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
	throw(fmt::format("VkResult failure {} {} {}", vkResultString(res).c_str(), originator, sourceLocation));
}

VkResult CheckVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
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

CmdBuffer::~CmdBuffer()
{
	SetState(CmdBufferState::Undefined);
	if (m_vkDevice != nullptr)
	{
		if (buf != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
		}
		if (pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(m_vkDevice, pool, nullptr);
		}
		if (execFence != VK_NULL_HANDLE)
		{
			vkDestroyFence(m_vkDevice, execFence, nullptr);
		}
	}
	buf = VK_NULL_HANDLE;
	pool = VK_NULL_HANDLE;
	execFence = VK_NULL_HANDLE;
	m_vkDevice = nullptr;
}

std::string CmdBuffer::StateString(CmdBufferState s)
{
	switch (s)
	{
	case CmdBufferState::Undefined:
		return "Undefined";
	case CmdBufferState::Initialized:
		return "Initialized";
	case CmdBufferState::Recording:
		return "Recording";
	case CmdBufferState::Executable:
		return "Executable";
	case CmdBufferState::Executing:
		return "Executing";
	default:
		break;
	}
	return "(Unknown)";
}

bool CmdBuffer::Init(VkDevice device, uint32_t queueFamilyIndex) {
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

bool teleport::android::CmdBuffer::Begin()
{
	CHECK_CBSTATE(CmdBufferState::Initialized);
	VkCommandBufferBeginInfo cmdBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	CHECK_VKCMD(vkBeginCommandBuffer(buf,&cmdBeginInfo));
	SetState(CmdBufferState::Recording);
	return true;
}

bool teleport::android::CmdBuffer::End()
{
	CHECK_CBSTATE(CmdBufferState::Recording);
	CHECK_VKCMD(vkEndCommandBuffer(buf));
	SetState(CmdBufferState::Executable);
	return true;
}

bool teleport::android::CmdBuffer::Exec(VkQueue queue)
{
	CHECK_CBSTATE(CmdBufferState::Executable);

	VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.commandBufferCount=1;
	submitInfo.pCommandBuffers=&buf;
	CHECK_VKCMD(vkQueueSubmit(queue,1,&submitInfo,execFence));

	SetState(CmdBufferState::Executing);
	return true;
}

bool teleport::android::CmdBuffer::Wait()
{
	// Waiting on a not-in-flight command buffer is a no-op
	if(state==CmdBufferState::Initialized)
	{
		return true;
	}

	CHECK_CBSTATE(CmdBufferState::Executing);

	const uint32_t timeoutNs=1*1000*1000*1000;
	for(int i=0; i < 5; ++i)
	{
		auto res=vkWaitForFences(m_vkDevice,1,&execFence,VK_TRUE,timeoutNs);
		if(res==VK_SUCCESS)
		{
			// Buffer can be executed multiple times...
			SetState(CmdBufferState::Executable);
			return true;
		}
		std::cout<<"Waiting for CmdBuffer fence timed out, retrying...";
	}

	return false;
}

bool teleport::android::CmdBuffer::Reset()
{
	if(state!=CmdBufferState::Initialized)
	{
		CHECK_CBSTATE(CmdBufferState::Executable);

		CHECK_VKCMD(vkResetFences(m_vkDevice,1,&execFence));
		CHECK_VKCMD(vkResetCommandBuffer(buf,0));

		SetState(CmdBufferState::Initialized);
	}

	return true;
}
