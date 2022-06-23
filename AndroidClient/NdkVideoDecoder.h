#include <media/NdkMediaCodec.h>
#include <media/NdkImageReader.h>
#include <vector>
#include <libavstream/common.hpp>
#include "Platform/CrossPlatform/VideoDecoder.h"

namespace platform
{
	namespace crossplatform
	{
		class Texture;
	}
}
class VideoDecoderBackend;

class NdkVideoDecoder //: SurfaceTexture.OnFrameAvailableListener
{
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
protected:
	platform::crossplatform::RenderPlatform* renderPlatform=nullptr;
	VideoDecoderBackend *videoDecoder=nullptr;
	avs::VideoCodec mCodecType;
	AMediaCodec * mDecoder = nullptr;
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
	int nextPayloadFlags=0;
	struct InputBuffer
	{
		int32_t inputBufferId=-1;
		size_t offset=0;
		int flags=-1;
	};
	std::vector<InputBuffer> nextInputBuffers;
	size_t nextInputBufferIndex=0;
};