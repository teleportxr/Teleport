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
	void initialize(platform::crossplatform::Texture* texture);
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
	VideoDecoderBackend *videoDecoder=nullptr;
	avs::VideoCodec mCodecType;
	AMediaCodec * mDecoder = nullptr;
	AImageReader *imageReader=nullptr;
	bool mDecoderConfigured = false;
	int mDisplayRequests = 0;

	ssize_t queueInputBuffer(std::vector<uint8_t> &ByteArray, int flags);

	int releaseOutputBuffer(bool render ) ;
	std::function<void(VideoDecoderBackend *)> nativeFrameAvailable;
	void onFrameAvailable(void* SurfaceTexture)
	{
		nativeFrameAvailable(videoDecoder);
	}

	const char *getCodecMimeType();
	platform::crossplatform::VideoDecoderParams videoDecoderParams;
};