// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include "AudioCommon.h"

namespace sca
{
	/*! A class to play audio from streams and files
	*/
	class AudioPlayer 
	{
	public:
		virtual ~AudioPlayer();
		virtual Result initializeAudioDevice() = 0;
		virtual Result configure(const AudioParams& audioParams) = 0;
		virtual Result deconfigure() = 0;
		virtual Result playStream(const uint8_t* data, size_t dataSize) = 0;
		bool isInitialized() { return initialized; }
		bool isConfigured() { return configured; }

	protected:
		AudioPlayer();
		AudioParams audioParams;
		bool initialized;
		bool configured;
	};
}

