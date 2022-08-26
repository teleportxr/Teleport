#include <media/NdkMediaCodec.h>
#include <media/NdkImageReader.h>
#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.hpp>
#include <vector>
#include <libavstream/common.hpp>
#include <thread>
#include <android/sync.h>
#include "Platform/CrossPlatform/VideoDecoder.h"

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

class NdkVideoDecoder //: SurfaceTexture.OnFrameAvailableListener
{
	platform::vulkan::Texture* targetTexture=nullptr;
public:
	NdkVideoDecoder(VideoDecoderBackend *d,avs::VideoCodec codecType);
	void initialize(platform::crossplatform::RenderPlatform* p,platform::crossplatform::Texture* texture);
	void shutdown();
	bool decode(std::vector<uint8_t> &ByteBuffer, avs::VideoPayloadType p, bool lastPayload);
	bool display();
	// callbacks:
	void onAsyncInputAvailable(AMediaCodec *codec,
		int32_t index);
	void onAsyncOutputAvailable(AMediaCodec *codec,
		int32_t index,
		AMediaCodecBufferInfo *bufferInfo);
	void onAsyncFormatChanged(	AMediaCodec *codec,
		AMediaFormat *format);
	void onAsyncError(AMediaCodec *codec,
        media_status_t error,
        int32_t actionCode,
        const char *detail);

	void onAsyncImageAvailable(AImageReader *reader);

		// processing thread:
	void processBuffersOnThread();
	void processInputBuffers();
	void processOutputBuffers();
	void processImages();
	void CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext);
protected:
	void FreeTexture(int index);
	int acquireFenceFd=0;
    std::mutex _mutex;
	struct ReflectedTexture
	{
		platform::vulkan::Texture* sourceTexture=nullptr;
		AImage *nextImage=nullptr;
		vk::Image videoSourceVkImage;
		vk::DeviceMemory mMem;
	};
	std::vector<ReflectedTexture> reflectedTextures;
	int reflectedTextureIndex=-1;
	std::atomic<int> nextImageIndex=-1;
	std::atomic<int> freeImageIndex=-1;
	std::thread *processBuffersThread= nullptr;

	platform::vulkan::RenderPlatform* renderPlatform=nullptr;
	VideoDecoderBackend *videoDecoder=nullptr;
	avs::VideoCodec mCodecType;
	AMediaCodec *mDecoder = nullptr;
	AImageReader *imageReader=nullptr;
	bool mDecoderConfigured = false;
	int mDisplayRequests = 0;

	int32_t queueInputBuffer(std::vector<uint8_t> &ByteArray, int flags,bool send);

	int releaseOutputBuffer(bool render ) ;
	std::function<void(VideoDecoderBackend *)> nativeFrameAvailable;
	void onFrameAvailable(void* SurfaceTexture)
	{
		nativeFrameAvailable(videoDecoder);
	}

	const char *getCodecMimeType();
	platform::crossplatform::VideoDecoderParams videoDecoderParams;
	std::vector<uint8_t> inputBufferToBeQueued;
	std::vector<int> texturesToFree;
	int nextPayloadFlags=0;
	struct DataBuffer
	{
		std::vector<uint8_t> bytes;
		int flags=-1;
		bool send=false;
	};
	std::vector<DataBuffer> dataBuffers;
	struct InputBuffer
	{
		int32_t inputBufferId=-1;
		size_t offset=0;
		int flags=-1;
	};
	std::vector<InputBuffer> nextInputBuffers;
	size_t nextInputBufferIndex=0;

	struct OutputBuffer
	{
		int32_t outputBufferId=-1;
		int32_t offset=0;
		int32_t size=0;
		int64_t presentationTimeUs=0;
		uint32_t flags=-1;
	};
	std::vector<OutputBuffer> outputBuffers;
};