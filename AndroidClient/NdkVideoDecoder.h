#include <media/NdkMediaCodec.h>
#include <vector>

enum class VideoCodec
{
	H264,
	H265,
	INVALID
};

enum class PayloadType
{
	FirstVCL,		/*!< Video Coding Layer unit (first VCL in an access unit). */
	VCL,			/*!< Video Coding Layer unit (any subsequent in each access unit). */
	VPS,			/*!< Video Parameter Set (HEVC only) */
	SPS,			/*!< Sequence Parameter Set */
	PPS,			/*!< Picture Parameter Set */
	ALE,			/*!< Custom name. NAL unit with alpha layer encoding metadata (HEVC only). */
	OtherNALUnit,	/*!< Other NAL unit. */
	AccessUnit		/*!< Entire access unit (possibly multiple NAL units). */
};

class VideoDecoder;

class NdkVideoDecoder //: SurfaceTexture.OnFrameAvailableListener
{
	NdkVideoDecoder(VideoDecoder *d,int codecType);
	VideoDecoder *videoDecoder=nullptr;
	int mCodecTypeIndex;
	AMediaCodec * mDecoder = nullptr;
	bool mDecoderConfigured = false;
	int mDisplayRequests = 0;

	void initialize(void* SurfaceTexture, int frameWidth , int frameHeight);

	void shutdown();

	bool decode(std::vector<uint8_t> &ByteBuffer, int payloadTypeIndex, bool lastPayload);
	bool display();

	ssize_t queueInputBuffer(std::vector<uint8_t> &ByteArray, int flags);

	int releaseOutputBuffer(bool render ) ;
	std::function<void(VideoDecoder *)> nativeFrameAvailable;
	void onFrameAvailable(void* SurfaceTexture)
	{
		nativeFrameAvailable(videoDecoder);
	}

	VideoCodec getCodecType();
	const char *getCodecMimeType();
	PayloadType getPayloadTypeFromIndex(int payloadTypeIndex) ;
};