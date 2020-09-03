#include "AudioStreamTarget.h"
#include "AudioPlayer.h"

namespace sca
{
	AudioStreamTarget::AudioStreamTarget(AudioPlayer* ap)
		: player(ap) {}

	avs::Result AudioStreamTarget::process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType payloadType)
	{
		//const float* data = reinterpret_cast<float*>(buffer);
		//auto r = player->playStream()
		return avs::Result::OK;
	}
}
