// libavstream
// (c) Copyright 2018-2021 Simul Software Ltd

#include <libavstream/httputil.hpp>
#include <logger.hpp>
#include <iostream>
#include <functional>
#include <filesystem>
#include <vector>
#include <curl/curl.h>
#include <string>
#include <fstream>
#include <fmt/chrono.h>
#include "Platform/Core/FileLoader.h"

using namespace std::chrono;
using namespace std::filesystem;
#ifdef __ANDROID__
#include <experimental/filesystem>
using namespace std::experimental::filesystem;
#define FILE_TIME_TYPE _FilesystemClock
#else
#define FILE_TIME_TYPE _File_time_clock
#endif
using namespace std::string_literals;

#define CURL_CHECK(cc) {if (cc != CURLE_OK){std::cerr<<"CURL code failed.\n";}}

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
		cacheDirectory=config.cacheDirectory;
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
						long response_code;
						curl_easy_getinfo(msgs[0].easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
						if(response_code==200)
						{
							size_t dataSize = transfer.getReceivedDataSize();
							transfer.mRequest.callbackFn(transfer.getReceivedData(), dataSize);
							if(transfer.mRequest.shouldCache)
								CacheReceivedFile(transfer);
						}
						else if(response_code==304)
						{
						// This means the file has not been updated since the cached version was saved. So we will send the cached version to the callback.
							auto *fileLoader = platform::core::FileLoader::GetFileLoader();
							if (fileLoader)
							{
								uint8_t *ptr=nullptr;
								uint32_t bytes=0;
								fileLoader->AcquireFileContents(((void*&)ptr),bytes,transfer.mRequest.cachedFilePath.c_str(),false);
								if(ptr&&bytes>0)
								{
									transfer.mRequest.callbackFn(ptr, bytes);
								}
							}
						}
					}
					else
					{
						AVSLOG(Error)<<"CURL transfer failed. \n";
					}
					transfer.stop();
				}
			}
		}

		return Result::OK;
	}
	std::string HTTPUtil::URLToFilePath(std::string url)
	{
		size_t protocol_end=url.find("://");
		std::string filepath=url.substr(protocol_end+3,url.length()-protocol_end-3);
		size_t first_slash=filepath.find("/");
		if(first_slash>=filepath.length())
			first_slash=filepath.length();
		std::string base_url=filepath.substr(0,first_slash);
		filepath=filepath.substr(first_slash,filepath.length()-first_slash);
		size_t colon_pos=base_url.find(":");
		if(colon_pos<base_url.length())
			base_url=base_url.substr(0,colon_pos);
		filepath=base_url+filepath;
		path fullpath = path(cacheDirectory) / filepath;
		// TODO: check path length is not too long.
		return fullpath.generic_string();
	}
	void HTTPUtil::CacheReceivedFile(const Transfer &transfer)
	{
		if(fileCacheSize +transfer.getReceivedDataSize()>maximumFileCacheSize)
		{
			return;
		}
		path fullPath = path(transfer.mRequest.cachedFilePath);
		std::filesystem::create_directories(fullPath.parent_path());
		if(transfer.getReceivedDataSize()>10000000000)
			return;
		auto *fileLoader = platform::core::FileLoader::GetFileLoader();
		if (!fileLoader)
			return;
		fileLoader->Save(transfer.getReceivedData(), (uint32_t)transfer.getReceivedDataSize(), transfer.mRequest.cachedFilePath.c_str(), false);
		fileCacheSize += transfer.getReceivedDataSize();
	}

	void HTTPUtil::CheckForCachedFile(HTTPPayloadRequest &request)
	{
		if (!request.shouldCache)
			return;
		
		request.cachedFilePath = URLToFilePath(request.url);
		if (std::filesystem::exists(request.cachedFilePath))
		{
			request.cached = true;
			file_time_type write_time = std::filesystem::last_write_time(path(request.cachedFilePath));
			auto sctp = time_point_cast<system_clock::duration>(write_time - FILE_TIME_TYPE::now() + system_clock::now());
			//auto sctp = FILE_TIME_TYPE::time_since_epoch(write_time);
			request.cacheUpdated = std::chrono::floor<seconds>(sctp);
		}
		else
			request.cached = false;
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
				HTTPPayloadRequest rq=request;
				CheckForCachedFile(rq);
				transfer.start(rq);
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
	
	std::string HTTPUtil::cert_path;
	HTTPUtil::Transfer::Transfer(CURLM* multi, std::string remoteURL, size_t bufferSize)
		: mMulti(multi)
		, mCurrentSize(0)
		, mActive(false)
		, mRemoteURL(remoteURL)
	{
		mHandle = curl_easy_init();
		mBuffer.resize(bufferSize);
		// Set to 1 to verify SSL certificates.
	#ifdef __ANDROID__
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYPEER, 0L));
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYHOST, 0L));
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_WRITEFUNCTION, &HTTPUtil::writeCallback));
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_WRITEDATA, this));
		// HTTP/2 please 
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0));
		// Wait for pipe connection to confirm 
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_PIPEWAIT, 1L)); 
		// Use CURLOPT_SSLCERT later.
		// see https://github.com/gcesarmza/curl-android-ios/issues/5
		// The solution is to download cacert.pem from the cURL site and distribute it with your app,
		//inside the APK. Then on runtime, copy it to a reachable directory, for instance the dataDir returned by the getApplicationInfo() function. The path to that file is what you pass to curl_setopt(CURLOPT_CAPATH)
		// https://curl.haxx.se/ca/cacert.pem
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_CAPATH, HTTPUtil::GetCertificatePath())); 
		CURL_CHECK(curl_easy_setopt(mHandle, CURLOPT_CAINFO, HTTPUtil::GetCertificatePath()));
	#else
		// Set to 1 to verify SSL certificates.
		curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(mHandle, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(mHandle, CURLOPT_WRITEFUNCTION, &HTTPUtil::writeCallback);
		curl_easy_setopt(mHandle, CURLOPT_WRITEDATA, this);
		// HTTP/2 please 
		curl_easy_setopt(mHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
		// Wait for pipe connection to confirm 
		curl_easy_setopt(mHandle, CURLOPT_PIPEWAIT, 1L); 
		// Use CURLOPT_SSLCERT later.
	#endif
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
			mCurrentSize = 0;
			mRequest = request;
			struct curl_slist *headerlist =nullptr;
			if (request.cached)
			{
				headerlist = curl_slist_append(NULL, getModifiedSinceHeader().c_str());
			}
			if(headerlist)
				curl_easy_setopt(mHandle, CURLOPT_HTTPHEADER, headerlist);
			//curl_easy_setopt(mHandle, CURLOPT_READDATA, this);
			//curl_easy_setopt(mHandle, CURLOPT_READFUNCTION, read_callback);
			CURLMcode rm = curl_multi_add_handle(mMulti, mHandle);
			if (rm != CURLM_OK)
			{
				return;
			}
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

	std::string HTTPUtil::Transfer::getModifiedSinceHeader()
	{
		//	If-Modified-Since: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT
		// e.g. If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT
		// TODO: because fmt::format with a time_point adds a silly number of decimal places to the "seconds", we can't use it for the whole time.
		// instead, we must extract the seconds separately.
		std::string header=fmt::format("If-Modified-Since: {:%a, %d %b %Y %H:%M}:00 GMT", mRequest.cacheUpdated,0);
		write(header.c_str(),header.length());
		return header;
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