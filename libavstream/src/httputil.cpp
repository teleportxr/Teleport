// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#include <libavstream/httputil.hpp>
#include <logger.hpp>
#include <iostream>
#include <functional>
#include <vector>
#include <curl/curl.h>

namespace avs 
{
	HTTPUtil::HTTPUtil()
		:  mTransferIndex(0)
		, mInitialized(false)
	{
		
	}

	HTTPUtil::~HTTPUtil()
	{
		shutdown();
	}

	Result HTTPUtil::initialize(const HTTPUtilConfig& config)
	{
		if (mInitialized)
		{
			return Result::OK;
		}

		curl_global_init(CURL_GLOBAL_DEFAULT);

		mMultiHandle=curl_multi_init();
		if (!mMultiHandle)
		{
			AVSLOG(Error) << "Failed to create CURL multi object";
			return Result::HTTPUtil_InitError;
		} 
		// Multiplexing allows for multiple transfers to be executed on a single TCP connection.
		// A server that supports the HTTP/2 protocol is required for multiplexing. 
		curl_multi_setopt(mMultiHandle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX); 

		std::string protocol = config.useSSL ? "https" : "http";
		
		//mRemoteURL = protocol + "://" + config.remoteIP + ":" + std::to_string(config.remoteHTTPPort) + "/";

		// Create easy Handles
		mTransfers.reserve(config.maxConnections);

		for (uint32_t i = 0; i < config.maxConnections; ++i)
		{
			mTransfers.emplace_back(mMultiHandle, "", mMinTransferBufferSize);
			mHandleTransferMap[mTransfers[i].getHandle()] = i;
		}

		mConfig = config;

		mInitialized = true;

		return Result::OK;
	}

	Result HTTPUtil::process()
	{
		if (!mInitialized)
		{
			return Result::HTTPUtil_NotInitialized;
		}

		// Try find a transfer but if none available, wait until next time.
		while (!mRequestQueue.empty())
		{
			if (AddRequest(mRequestQueue.front()))
			{
				mRequestQueue.pop();
			}
			else
			{
				break;
			}
		}

		int runningHandles;
		curl_multi_wait ( mMultiHandle, NULL, 0, 0, NULL);
		CURLMcode r = curl_multi_perform(mMultiHandle, &runningHandles);
		if (r != CURLM_OK)
		{
			AVSLOG(Error) << "CURL multi perform failed.";
			return Result::HTTPUtil_TransferError;
		}

		//if (runningHandles > 0)
		{
			int numMsgs;
			CURLMsg* msgs = curl_multi_info_read(mMultiHandle, &numMsgs);

			if(msgs)
			{
				if (msgs[0].msg == CURLMSG_DONE)
				{
					int index = mHandleTransferMap[msgs[0].easy_handle];
					Transfer& transfer = mTransfers[index];
					if (msgs[0].data.result == CURLE_OK)
					{
						size_t dataSize = transfer.getReceivedDataSize();
						transfer.mRequest.callbackFn(transfer.getReceivedData(), dataSize);
					}
					transfer.stop();
				}
			}
		}

		return Result::OK;
	}

	bool HTTPUtil::AddRequest(const HTTPPayloadRequest& request)
	{
		// Try all transfers starting from the last.
		for (uint32_t i = 0; i < mConfig.maxConnections; ++i)
		{
			int index = (mTransferIndex + i) % mConfig.maxConnections;
			Transfer& transfer = mTransfers[index];
			if (!transfer.isActive())
			{
				transfer.start(request);
				mTransferIndex = (index + 1) % mConfig.maxConnections;
				return true;
			}
		}
		return false;
	}

	Result HTTPUtil::shutdown()
	{
		if (!mInitialized)
		{
			return Result::HTTPUtil_NotInitialized;
		}

		mHandleTransferMap.clear();

		mTransfers.clear();
		curl_multi_cleanup(mMultiHandle);
		mMultiHandle=nullptr;
		curl_global_cleanup();

		while (!mRequestQueue.empty())
		{
			mRequestQueue.pop();
		}

		mInitialized = false;

		return Result::OK;
	}

	size_t HTTPUtil::writeCallback(char* ptr, size_t size, size_t nmemb, void* userData)
	{
		size_t realSize = size * nmemb;

		Transfer* transfer = reinterpret_cast<Transfer*>(userData);
		transfer->write(ptr, realSize);

		return realSize;
	}

	HTTPUtil::Transfer::Transfer(CURLM* multi, std::string remoteURL, size_t bufferSize)
		: mMulti(multi)
		, mCurrentSize(0)
		, mActive(false)
		, mRemoteURL(remoteURL)
	{
		mHandle = curl_easy_init();
		mBuffer.resize(bufferSize);
		// Set to 1 to verify SSL certificates.
		curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYHOST, 1L);
		curl_easy_setopt(mHandle, CURLOPT_WRITEFUNCTION, &HTTPUtil::writeCallback);
		curl_easy_setopt(mHandle, CURLOPT_WRITEDATA, this);
		// HTTP/2 please 
		curl_easy_setopt(mHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
		// Wait for pipe connection to confirm 
		curl_easy_setopt(mHandle, CURLOPT_PIPEWAIT, 1L); 
		// Use CURLOPT_SSLCERT later.
	}

	HTTPUtil::Transfer::~Transfer()
	{
		stop();
		curl_easy_cleanup(mHandle);
	}
	size_t read_callback(char *buffer, size_t size, size_t nitems, void *t)
	{
		HTTPUtil::Transfer *transfer=(HTTPUtil::Transfer*)t;
		//transfer->getReceivedData
		return size;
	}
	// Adds to multi stack.
	void HTTPUtil::Transfer::start(const HTTPPayloadRequest& request)
	{
		if (!mActive)
		{
			std::string url = request.url;
			CURLcode re = curl_easy_setopt(mHandle, CURLOPT_URL, url.c_str());
			if (re != CURLE_OK)
			{
				return;
			}
			//curl_easy_setopt(mHandle, CURLOPT_READDATA, this);
			//curl_easy_setopt(mHandle, CURLOPT_READFUNCTION, read_callback);
			CURLMcode rm = curl_multi_add_handle(mMulti, mHandle);
			if (rm != CURLM_OK)
			{
				return;
			}
			mCurrentSize = 0;
			mRequest = request;
		//	addBufferHeader();
			mActive = true;
		}
	}

	// Removes from multi stack.
	void HTTPUtil::Transfer::stop()
	{
		if (mActive)
		{
			curl_multi_remove_handle(mMulti, mHandle);
			mActive = false;

			FilePayloadInfo* info = (FilePayloadInfo*)mBuffer.data();
			// data size includes size of the file name and the file name.
			info->dataSize = mCurrentSize - sizeof(FilePayloadInfo);
		}
	}

	void HTTPUtil::Transfer::addBufferHeader()
	{
		FilePayloadInfo payloadInfo;
		// Set this later
		payloadInfo.dataSize = 0;
		payloadInfo.httpPayloadType = FilePayloadType::Mesh;
		// Size of file name in bytes.
		size_t fileNameSize = mRequest.url.size();
		memcpy(&mBuffer[0], &payloadInfo, sizeof(FilePayloadInfo));
		memcpy(&mBuffer[sizeof(FilePayloadInfo)], &fileNameSize, sizeof(size_t));
		memcpy(&mBuffer[sizeof(FilePayloadInfo) + sizeof(size_t)], mRequest.url.c_str(), fileNameSize);
		mCurrentSize += sizeof(FilePayloadInfo) + sizeof(size_t) + fileNameSize;
	}

	void HTTPUtil::Transfer::write(const char* data, size_t dataSize)
	{
		size_t newSize = mCurrentSize + dataSize;
		if (newSize > mBuffer.size())
		{
			mBuffer.resize(newSize);
		}
		memcpy(&mBuffer[mCurrentSize], data, dataSize);
		mCurrentSize = newSize;
	}
} 