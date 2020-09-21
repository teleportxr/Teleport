// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

//C Libraries
#include <iostream>
#include <string>
#include <assert.h>

//STL
#include <vector>
#include <deque>
#include <map>
#include <queue>
#include <mutex>
#include <memory>
#include <algorithm>
#include <functional>

//Debug
#define SCA_CERR_BREAK(msg, errCode) std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define SCA_CERR(msg)				 std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;

#define SCA_COUT_BREAK(msg, errCode) std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define SCA_COUT(msg)				 std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;

extern void log_print(const char* source,const char *format, ...);

#if defined(__ANDROID__)
#include <android/log.h>
#define FILE_LINE (std::string(__FILE__) + std::string("(") +  std::to_string(__LINE__) + std::string("):")).c_str()
#define SCA_LOG(...) __android_log_print(ANDROID_LOG_INFO, "SCA", __VA_ARGS__);
#else
#define SCA_LOG(fmt,...) log_print( "SCA", fmt, __VA_ARGS__);
#endif

namespace sca
{
	/*!
	 * Result type.
	 */
	struct Result
	{
		/*! Result code enumeration. */
		enum Code
		{
			OK = 0,
			UnknownError,
			AudioPlayerIsNull,
			AudioDeviceInitializationError,
			AudioMasteringVoiceCreationError,
			AudioSourceVoiceCreationError,
			AudioPlayingError,
			AudioPlayerAlreadyInitialized,
			AudioPlayerNotInitialized,
			AudioPlayerAlreadyConfigured,
			AudioPlayerNotConfigured,
			AudioPlayerBufferSubmissionError,
			AudioStreamBuilderCreationError,
			AudioStreamBuilderDeletionError,
			AudioOpenStreamError,
			AudioCloseStreamError,
			AudioReleaseStreamError
		};

		Result() : m_code(Code::OK) 
		{}
		Result(Code code) : m_code(code)
		{}
		//! if(Result) returns true only if m_code == OK i.e. is ZERO.
		operator bool() const { return m_code == Code::OK; }
		operator Code() const { return m_code; }
	private:
		Code m_code;
	};

	enum class AudioCodec
	{
		Any = 0,
		Invalid = 0,
		PCM
	};
	struct AudioParams
	{
		AudioCodec codec = AudioCodec::PCM;
		uint32_t sampleRate = 44100;
		uint32_t bitsPerSample = 16;
		uint32_t numChannels = 2;
	};

	template<class T>
	class ThreadSafeQueue
	{
	public:
		void push(T& val)
		{
			std::lock_guard<std::mutex> guard(mutex);
			data.push_back(val);
		}

		void push(T&& val)
		{
			std::lock_guard<std::mutex> guard(mutex);
			data.push_back(std::move(val));

		}

		void pop()
		{
			std::lock_guard<std::mutex> guard(mutex);
			data.pop();
		}

		T& front()
		{
			std::lock_guard<std::mutex> guard(mutex);
			return data.front();
		}

		T& back()
		{
			std::lock_guard<std::mutex> guard(mutex);
			return data.back();
		}

		template <class... _Valty>
		T& emplace(_Valty&&... _Val)
		{
			std::lock_guard<std::mutex> guard(mutex);
#if _HAS_CXX17
			return data.emplace(std::forward<_Valty>(_Val)...);
#else // ^^^ C++17 or newer / C++14 vvv
			data.emplace(std::forward<_Valty>(_Val)...);
			return data.back();
#endif // _HAS_CXX17
		}

	private:
		std::mutex mutex;
		std::queue<T> data;
	};

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if(p) { delete p; (p)=NULL; } }
#endif

}