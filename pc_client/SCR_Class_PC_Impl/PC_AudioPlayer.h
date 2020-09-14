// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "crossplatform/AudioPlayer.h"
#include <wrl.h>
#include <queue>
#include <mutex>
#include <future>
#include "xaudio2.h"

struct AudioBuffer
{
	std::vector<BYTE> data;
	XAUDIO2_BUFFER buffer;
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

//interface IXAudio2;
//interface IXAudio2MasteringVoice;
//interface IXAudio2SourceVoice;

/*! A class to play audio from streams and files for PC
*/
class PC_AudioPlayer final : public sca::AudioPlayer
{
public:
	PC_AudioPlayer();
	~PC_AudioPlayer();

	sca::Result playStream(const uint8_t* data, size_t dataSize) override;

	sca::Result initializeAudioDevice() override;

	sca::Result configure(const sca::AudioParams& audioParams) override;

	sca::Result deconfigure() override;

private:
	sca::Result asyncInitializeAudioDevice();

	std::future<sca::Result> initResult;

	Microsoft::WRL::ComPtr<IXAudio2> device;						
	IXAudio2MasteringVoice* masteringVoice;
	IXAudio2SourceVoice* sourceVoice;

	ThreadSafeQueue<AudioBuffer> audioBuffers;

	std::unique_ptr<class VoiceCallback> voiceCallback;
};


