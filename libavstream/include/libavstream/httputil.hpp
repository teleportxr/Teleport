// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <platform.hpp>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <curl/curl.h>
#include <unordered_map>

namespace avs
{
	using MultiHandle = std::unique_ptr<CURLM, std::function<void(CURLM*)>>;

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

	private:
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
			const char* getReceivedData() const { return mBuffer.data(); }
			size_t getReceivedDataSize() const { return mCurrentSize; }
		private:
			HTTPPayloadRequest mRequest;
			CURL* mHandle;
			CURLM* mMulti;
			size_t mCurrentSize;
			bool mActive;
			std::string mRemoteURL;
			std::vector<char> mBuffer;
		};

	public:
		HTTPUtil();
		~HTTPUtil();

		Result initialize(const HTTPUtilConfig& config, std::function<void(const char* buffer, size_t bufferSize)>&& receiveCallback);
		Result process();
		Result shutdown();

		std::queue<HTTPPayloadRequest>& GetRequestQueue() { return mRequestQueue; } 

	private:
		bool AddRequest(const HTTPPayloadRequest& request);
		static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userData);

		HTTPUtilConfig mConfig;
		std::string mRemoteURL;
		MultiHandle mMultiHandle;
		std::vector<Transfer> mTransfers;
		std::queue<HTTPPayloadRequest> mRequestQueue;
		std::unordered_map<CURL*, int> mHandleTransferMap;
		std::function<void(const char* buffer, size_t bufferSize)> mReceiveCallback;
		int mTransferIndex;
		bool mInitialized;

		static constexpr size_t mMinTransferBufferSize = 300000; //bytes
	};
} 