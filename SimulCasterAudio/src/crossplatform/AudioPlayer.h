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
		virtual Result playStream(const float* data, size_t dataSize, AudioType audioType) = 0;

	protected:
		AudioPlayer();
		virtual Result initalize() = 0;
	};
}

