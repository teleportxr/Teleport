// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <stream/parser_avc.hpp>
#include <stream/parser_geo.hpp>

using namespace avs;

StreamParserInterface *StreamParserInterface::Create(StreamParserType type)
{
	switch (type)
	{
	case StreamParserType::AVC_AnnexB:
		return new StreamParserAVC;
	case StreamParserType::Geometry:
		return new GeometryStreamParser;
	case StreamParserType::Custom:
		break;
	}
	return nullptr;
}

