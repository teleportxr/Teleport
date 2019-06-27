#include "GeometryDecoder.h"

using namespace avs;

GeometryDecoder::GeometryDecoder()
{
}


GeometryDecoder::~GeometryDecoder()
{
}

template<typename T> size_t get(const T * &data)
{
	const T& t = *data;
	data ++ ;
	return t;
}
template<typename T> void get(T* target,const uint8_t *data, size_t count)
{
	memcpy(target, data, count*sizeof(T));
}

avs::Result GeometryDecoder::decode(const void * buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface * target)
{

	//get((const uint8_t*)buffer);
	return avs::Result::OK;
}
