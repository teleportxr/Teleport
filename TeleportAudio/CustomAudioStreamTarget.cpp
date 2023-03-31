#include "CustomAudioStreamTarget.h"

using namespace teleport::audio;

CustomAudioStreamTarget::CustomAudioStreamTarget()
{

}
CustomAudioStreamTarget::~CustomAudioStreamTarget()
{
	
}
void CustomAudioStreamTarget::SetPlayCallback(PlayCallback c)
{
	mPlayCallback = c;
}

avs::Result CustomAudioStreamTarget::deconfigure()
{
	return avs::Result::OK;
}

avs::Result CustomAudioStreamTarget::process(const void* buffer, size_t bufferSizeInBytes, avs::AudioPayloadType )
{
	const auto data = (uint8_t*)(buffer);
	mPlayCallback(data, bufferSizeInBytes);

	return avs::Result::OK;
}
