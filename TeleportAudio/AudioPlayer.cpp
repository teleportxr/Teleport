// (C) Copyright 2018-2020 Simul Software Ltd

#include "AudioPlayer.h"

using namespace teleport::audio;

AudioPlayer::AudioPlayer()
	: mInitialized(false)
	, mConfigured(false)
	, mRecording(false)
	, mRecordingAllowed(false)
	, mInputDeviceAvailable(false)
	, mLastResult(Result::OK)
{
}