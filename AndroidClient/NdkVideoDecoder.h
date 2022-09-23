#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.hpp>

#include <media/NdkMediaCodec.h>
#include <media/NdkImageReader.h>

#include <Platform/CrossPlatform/VideoDecoder.h>
#include <libavstream/common.hpp>

#include <vector>
#include <deque>
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
namespace teleport
{
	namespace android
	{
		class VideoDecoderBackend;

		class NdkVideoDecoder
		{
			//enum/structs
		protected:
			struct ReflectedTexture
			{
				platform::vulkan::Texture*	sourceTexture = nullptr;
				AImage*						nextImage = nullptr;
				vk::Image					videoSourceVkImage;
				vk::DeviceMemory			videoSourceVkDeviceMemory;
			};
			struct DataBuffer
			{
				std::vector<uint8_t>	bytes;
				int						flags = -1;
				bool					send = false;
			};
			struct InputBuffer
			{
				int32_t	inputBufferId = -1;
				size_t	offset = 0;
				int		flags = -1;
			};
			struct OutputBuffer
			{
				int32_t		outputBufferId = -1;
				int32_t		offset = 0;
				int32_t		size = 0;
				int64_t		presentationTimeUs = 0;
				uint32_t	flags = -1;
			};
			//%ANDROID_SDK_ROOT%\sources\android-29\android\media\MediaCodecInfo.java
			enum class CodecCapabilities
			{
				COLOR_FormatMonochrome = 1,
				COLOR_Format8bitRGB332 = 2,
				COLOR_Format12bitRGB444 = 3,
				COLOR_Format16bitARGB4444 = 4,
				COLOR_Format16bitARGB1555 = 5,
				COLOR_Format16bitRGB565 = 6,
				COLOR_Format16bitBGR565 = 7,
				COLOR_Format18bitRGB666 = 8,
				COLOR_Format18bitARGB1665 = 9,
				COLOR_Format19bitARGB1666 = 10,
				COLOR_Format24bitRGB888 = 11,
				COLOR_Format24bitBGR888 = 12,
				COLOR_Format24bitARGB1887 = 13,
				COLOR_Format25bitARGB1888 = 14,
				COLOR_Format32bitBGRA8888 = 15,
				COLOR_Format32bitARGB8888 = 16,
				COLOR_FormatYUV411Planar = 17,
				COLOR_FormatYUV411PackedPlanar = 18,
				COLOR_FormatYUV420Planar = 19,
				COLOR_FormatYUV420PackedPlanar = 20,
				COLOR_FormatYUV420SemiPlanar = 21,
				COLOR_FormatYUV422Planar = 22,
				COLOR_FormatYUV422PackedPlanar = 23,
				COLOR_FormatYUV422SemiPlanar = 24,
				COLOR_FormatYCbYCr = 25,
				COLOR_FormatYCrYCb = 26,
				COLOR_FormatCbYCrY = 27,
				COLOR_FormatCrYCbY = 28,
				COLOR_FormatYUV444Interleaved = 29,
				COLOR_FormatRawBayer8bit = 30,
				COLOR_FormatRawBayer10bit = 31,
				COLOR_FormatRawBayer8bitcompressed = 32,
				COLOR_FormatL2 = 33,
				COLOR_FormatL4 = 34,
				COLOR_FormatL8 = 35,
				COLOR_FormatL16 = 36,
				COLOR_FormatL24 = 37,
				COLOR_FormatL32 = 38,
				COLOR_FormatYUV420PackedSemiPlanar = 39,
				COLOR_FormatYUV422PackedSemiPlanar = 40,
				COLOR_Format18BitBGR666 = 41,
				COLOR_Format24BitARGB6666 = 42,
				COLOR_Format24BitABGR6666 = 43,
				COLOR_TI_FormatYUV420PackedSemiPlanar = 0x7f000100,
				COLOR_FormatSurface = 0x7F000789,
				COLOR_Format32bitABGR8888 = 0x7F00A000,
				COLOR_FormatYUV420Flexible = 0x7F420888,
				COLOR_FormatYUV422Flexible = 0x7F422888,
				COLOR_FormatYUV444Flexible = 0x7F444888,
				COLOR_FormatRGBFlexible = 0x7F36B888,
				COLOR_FormatRGBAFlexible = 0x7F36A888,
				COLOR_QCOM_FormatYUV420SemiPlanar = 0x7fa30c00,

				//OMX extensions
				//https://android.googlesource.com/platform/frameworks/native/+/kitkat-release/include/media/openmax/OMX_IVCommon.h
				
				OMX_COLOR_FormatAndroidOpaque = 0x7F000789,
				OMX_TI_COLOR_FormatYUV420PackedSemiPlanar = 0x7F000100,
				//OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00,
				//OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03,
				//OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m = 0x7FA30C04,
				OMX_SEC_COLOR_FormatNV12Tiled = 0x7FC00002,

				//https://review.carbonrom.org/plugins/gitiles/CarbonROM/android_hardware_qcom_media/+/fa202b9b18f17f7835fd602db5fff530e61112b4/msmcobalt/mm-core/inc/OMX_QCOMExtns.h

				OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00,
				OMX_QCOM_COLOR_FormatYVU420PackedSemiPlanar32m4ka = 0x7FA30C01,
				OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar16m2ka = 0x7FA30C02,
				OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03,
				OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m = 0x7FA30C04,
				OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32mMultiView = 0x7FA30C05,
				OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32mCompressed = 0x7FA30C06,
				OMX_QCOM_COLOR_Format32bitRGBA8888 = 0x7FA30C07,
				OMX_QCOM_COLOR_Format32bitRGBA8888Compressed = 0x7FA30C08
			};
			enum class BitRateMode
			{
				BITRATE_MODE_CQ = 0,	//Constant quality mode
				BITRATE_MODE_VBR = 1,	//Variable bitrate mode
				BITRATE_MODE_CBR = 2,	//Constant bitrate mode
			};

			//Methods
		public:
			NdkVideoDecoder(VideoDecoderBackend* d, avs::VideoCodec codecType);
			~NdkVideoDecoder();

			void Initialize(platform::crossplatform::RenderPlatform* p, platform::crossplatform::Texture* texture);
			void Shutdown();
			bool Decode(std::vector<uint8_t>& ByteBuffer, avs::VideoPayloadType p, bool lastPayload);
			bool Display();
			void CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext);

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
			int32_t QueueInputBuffer(std::vector<uint8_t>& ByteArray, int flags, bool send);
			int ReleaseOutputBuffer(bool render);
			const char* GetCodecMimeType();

			//Processing thread
		private:
			void processBuffersOnThread();
			void processInputBuffers();
			void processOutputBuffers();
			void processImages();

		protected:
			platform::vulkan::Texture* targetTexture = nullptr;
			int acquireFenceFd = 0;
			std::mutex texture_mutex;

			std::vector<ReflectedTexture> reflectedTextures;
			size_t reflectedTextureIndex = -1;
			std::atomic_int nextImageIndex = -1;
			std::atomic_int freeImageIndex = -1;
			std::vector<int> texturesToFree;

			std::thread* processBuffersThread = nullptr;
			std::mutex buffers_mutex;
			std::atomic_bool stopProcessBuffersThread = false;

			platform::vulkan::RenderPlatform* renderPlatform = nullptr;
			vk::PhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
			VideoDecoderBackend* videoDecoder = nullptr;
			avs::VideoCodec codecType;
			AMediaCodec* decoder = nullptr;
			AImageReader* imageReader = nullptr;
			bool decoderConfigured = false;
			int displayRequests = 0; //TODO: Remove maybe?

			platform::crossplatform::VideoDecoderParams videoDecoderParams;

			std::deque<DataBuffer> dataBuffers;
			std::vector<InputBuffer> nextInputBuffers;
			size_t nextInputBufferIndex = 0;
			std::deque<OutputBuffer> outputBuffers;
		};
	}
}