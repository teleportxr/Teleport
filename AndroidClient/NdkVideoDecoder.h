//Vulkan
#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.hpp>

//Android Media API
#include <media/NdkMediaCodec.h>
#include <media/NdkImageReader.h>

#include <Platform/CrossPlatform/VideoDecoder.h>
#include "Platform/CrossPlatform/VideoDecoder.h"
#include <libavstream/common.hpp>
#include <vector>
#include <thread>
#include <android/sync.h>

namespace platform
{
	namespace crossplatform
	{
		class Texture;
	}
	namespace vulkan
	{
		class Texture;
		class RenderPlatform;
	}
}
class VideoDecoderBackend;

class NdkVideoDecoder
{
	//enum/structs
protected:
	struct ReflectedTexture
	{
		platform::vulkan::Texture* sourceTexture = nullptr;
		AImage* nextImage = nullptr;
		vk::Image videoSourceVkImage;
		vk::DeviceMemory mMem;
	};
	struct DataBuffer
	{
		std::vector<uint8_t> bytes;
		int flags = -1;
		bool send = false;
	};
	struct InputBuffer
	{
		int32_t inputBufferId = -1;
		size_t offset = 0;
		int flags = -1;
	};
	struct OutputBuffer
	{
		int32_t outputBufferId = -1;
		int32_t offset = 0;
		int32_t size = 0;
		int64_t presentationTimeUs = 0;
		uint32_t flags = -1;
	};

	//Methods
public:
	NdkVideoDecoder(VideoDecoderBackend* d, avs::VideoCodec codecType);
	~NdkVideoDecoder();

	void Initialize(platform::crossplatform::RenderPlatform* p, platform::crossplatform::Texture* texture);
	void Shutdown();
	bool Decode(std::vector<uint8_t>& ByteBuffer, avs::VideoPayloadType p, bool lastPayload);
	bool Display();
	void CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext);
	
	//Callbacks
private:
	//Called when an input buffer becomes available.
	//The specified index is the index of the available input buffer.
	static void onAsyncInputAvailable(AMediaCodec* codec, void* userdata, int32_t index);
	
	//Called when an output buffer becomes available.
	//The specified index is the index of the available output buffer.
	//The specified bufferInfo contains information regarding the available output buffer.
	static void onAsyncOutputAvailable(AMediaCodec* codec, void* userdata, int32_t index, AMediaCodecBufferInfo* bufferInfo);

	//Called when the output format has changed.
	//The specified format contains the new output format.
	static void onAsyncFormatChanged(AMediaCodec* codec, void* userdata, AMediaFormat* format);

	//Called when the MediaCodec encountered an error.
	//The specified actionCode indicates the possible actions that client can take,
	//and it can be checked by calling AMediaCodecActionCode_isRecoverable or
	//AMediaCodecActionCode_isTransient. If both AMediaCodecActionCode_isRecoverable()
	//and AMediaCodecActionCode_isTransient() return false, then the codec error is fatal
	//and the codec must be deleted.
	//The specified detail may contain more detailed messages about this error.
	static void onAsyncError(AMediaCodec* codec, void* userdata, media_status_t error, int32_t actionCode, const char* detail);

	//This callback is called when there is a new image available in the image reader's queue.
	//The callback happens on one dedicated thread per AImageReader instance. It is okay
	//to use AImageReader_* and AImage_* methods within the callback. Note that it is possible that
	//calling AImageReader_acquireNextImage or AImageReader_acquireLatestImage
	//returns AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE within this callback. For example, when
	//there are multiple images and callbacks queued, if application called
	//AImageReader_acquireLatestImage, some images will be returned to system before their
	//corresponding callback is executed.
	static void onAsyncImageAvailable(void* context, AImageReader* reader);

	//Others
protected:
	void FreeTexture(int index);
	int32_t QueueInputBuffer(std::vector<uint8_t> &ByteArray, int flags,bool send);
	int ReleaseOutputBuffer(bool render ) ;
	const char* GetCodecMimeType();
	void OnFrameAvailable(void* SurfaceTexture) { nativeFrameAvailable(videoDecoder); }

	//Processing thread
private:
	void processBuffersOnThread();
	void processInputBuffers();
	void processOutputBuffers();
	void processImages();

protected:
	platform::vulkan::Texture* targetTexture = nullptr;;
	int acquireFenceFd = 0;
	std::mutex _mutex;

	std::vector<ReflectedTexture> reflectedTextures;
	int reflectedTextureIndex = -1;
	std::atomic<int> nextImageIndex = -1;
	std::atomic<int> freeImageIndex = -1;
	std::thread* processBuffersThread = nullptr;

	platform::vulkan::RenderPlatform* renderPlatform = nullptr;
	VideoDecoderBackend* videoDecoder = nullptr;
	avs::VideoCodec codecType;
	AMediaCodec* decoder = nullptr;
	AImageReader* imageReader = nullptr;
	bool decoderConfigured = false;
	int displayRequests = 0;

	std::function<void(VideoDecoderBackend*)> nativeFrameAvailable;

	platform::crossplatform::VideoDecoderParams videoDecoderParams;
	std::vector<uint8_t> inputBufferToBeQueued;
	std::vector<int> texturesToFree;
	int nextPayloadFlags = 0;

	std::vector<DataBuffer> dataBuffers;
	std::vector<InputBuffer> nextInputBuffers;
	size_t nextInputBufferIndex = 0;
	std::vector<OutputBuffer> outputBuffers;
};