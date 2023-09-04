// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>

typedef void CURL;
typedef void CURLM;
namespace avs
{
	typedef std::function<void(const uint8_t* buffer, size_t bufferSize)> HTTPCallbackFn;
	struct HTTPPayloadRequest
	{
		//FilePayloadType type=FilePayloadType::Invalid;
		std::string url;
		HTTPCallbackFn callbackFn;
	};

	struct HTTPUtilConfig
	{
		const char* remoteIP = "";
		int32_t remoteHTTPPort = 0;
		uint32_t connectionTimeout = 5000;
		uint32_t maxConnections = 10;
		bool useSSL = true;
	};
	/*!
	 * Utility for making HTTP/HTTPS requests.
	 */
	class AVSTREAM_API HTTPUtil
	{

	public:
		class Transfer
		{
		public:
			Transfer(CURLM* multi, std::string remoteURL, size_t bufferSize);
			~Transfer();
			void start(const HTTPPayloadRequest& request);
			void stop();
			void addBufferHeader();
			void write(const char* data, size_t dataSize);
			CURL* getHandle() const { return mHandle; };
			bool isActive() const { return mActive; }
			const uint8_t* getReceivedData() const { return mBuffer.data(); }
			size_t getReceivedDataSize() const { return mCurrentSize; }
			HTTPPayloadRequest mRequest;
			CURL* mHandle;
			CURLM* mMulti;
			size_t mCurrentSize;
			bool mActive;
			std::string mRemoteURL;
			std::vector<uint8_t> mBuffer;
		};

	public:
		HTTPUtil();
		~HTTPUtil();

		Result initialize(const HTTPUtilConfig& config);
		Result process();
		Result shutdown();
		static void SetCertificatePath(const char *p)
		{
			cert_path=p;
		}
		static const char*GetCertificatePath()
		{
		return cert_path.c_str();
		}
		std::queue<HTTPPayloadRequest>& GetRequestQueue() { return mRequestQueue; } 

	private:
		static std::string cert_path;
		bool AddRequest(const HTTPPayloadRequest& request);
		static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userData);

		HTTPUtilConfig mConfig;
		CURLM *mMultiHandle;
		std::vector<Transfer> mTransfers;
		std::queue<HTTPPayloadRequest> mRequestQueue;
		std::unordered_map<CURL*, int> mHandleTransferMap;
		int mTransferIndex;
		bool mInitialized;

		static constexpr size_t mMinTransferBufferSize = 300000; //bytes
	};
} 