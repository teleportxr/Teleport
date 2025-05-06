
#pragma once

#include "libavstream/common.hpp"

namespace teleport
{
	namespace core
	{
		//! Current state of a TextCanvas component.
		struct TextCanvas
		{
			avs::uid font_uid = 0;
			int pointSize = 0;
			float lineHeight = 0.0f;
			vec4 colour = { 1.f,1.f,1.f,1.f };
			std::string text;
		};
	}
}