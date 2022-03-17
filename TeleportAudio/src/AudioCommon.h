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
#include "ErrorHandling.h"

#define SCA_CERR TELEPORT_CERR
#define SCA_COUT TELEPORT_COUT

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
            AudioStreamCreationError,
            AudioStreamConfigurationError,
			AudioCloseStreamError,
			AudioResourceDeletionError,
			AudioOutputMixerInitializationError,
            AudioBufferInitializationError,
			AudioWriteError,
			AudioSetStateError,
			AudioRecorderCreationError,
			AudioRecorderInitializationError,
			AudioRecordingNotPermitted,
			AudioRecorderStartError,
			NoAudioInputDeviceFound,
			AudioProcessingError
		};

		Result() : m_code(Code::OK) 
		{}
		Result(Code code) : m_code(code)
		{}
		//! if(Result) returns true only if m_code == OK i.e. is ZERO.
		operator bool() const { return m_code == OK; }
		operator Code() const { return m_code; }
		bool operator ==(const Code & c) const { return m_code == c; }
		bool operator !=(const Code & c) const { return m_code != c; }
		bool operator ==(const Result& r) const { return m_code == r.m_code; }
		bool operator !=(const Result& r) const { return m_code != r.m_code; }
	private:
		Code m_code;
	};

	enum class AudioCodec
	{
		Any = 0,
		Invalid = 0,
		PCM
	};
	struct AudioSettings
	{
		AudioCodec codec = AudioCodec::PCM;
		uint32_t sampleRate = 44100;
		uint32_t bitsPerSample = 16;
		uint32_t numChannels = 2;
	};

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if(p) { delete p; (p)=NULL; } }
#endif

}