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
		virtual Result playStream(const uint8_t* data, size_t dataSize) = 0;
		virtual Result initialize(const AudioParams& audioParams);

	protected:
		AudioPlayer();
		AudioParams audioParams;
		bool initialized;
	};
}

