// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include <crossplatform/AudioPlayer.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <libavstream/common.hpp>

/*! A class to play audio from streams and files for PC
*/
class SL_AudioPlayer final : public sca::AudioPlayer
{
public:
	SL_AudioPlayer();
	~SL_AudioPlayer();

	sca::Result initializeAudioDevice() override;

	sca::Result configure(const sca::AudioSettings& audioSettings) override;

	sca::Result playStream(const uint8_t* data, size_t dataSize) override;

	sca::Result startRecording(std::function<void(const uint8_t * data, size_t dataSize)> recordingCallback) override;

    sca::Result processRecordedAudio() override;

	sca::Result stopRecording() override;

	sca::Result deconfigure() override;

	void onAudioProcessed() override;

	void onAudioRecorded(SLAndroidSimpleBufferQueueItf bq);

private:
	SLObjectItf mEngineObject = nullptr;
	SLEngineItf mEngineInterface  = nullptr;

	// Playing
    SLObjectItf mOutputMixObject = nullptr;
    SLObjectItf mPlayerObject = nullptr;
	SLPlayItf mPlayInterface = nullptr;
	SLAndroidSimpleBufferQueueItf mOutputBufferQueueInterface = nullptr;
	avs::ThreadSafeQueue<std::vector<uint8_t>> mOutputBufferQueue;

	// Recording
	SLObjectItf mRecorderObject = nullptr;
	SLRecordItf mRecordInterface = nullptr;
	std::unique_ptr<class RecordBuffer> mRecordBuffer;
	std::function<void(const uint8_t * data, size_t dataSize)> mRecordingCallback;

};


