// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <map>
#include <stdint.h>
#include <vector>

#include "libavstream/common_input.h"

namespace teleport
{
	namespace client
	{
		class Input
		{
		public:			
			std::vector<avs::InputEventBinary> binaryEvents;
			std::vector<avs::InputEventAnalogue> analogueEvents;
			std::vector<avs::InputEventMotion> motionEvents;

			void clear();
			void addBinaryEvent( avs::InputId inputID, bool activated);
			void addAnalogueEvent( avs::InputId inputID, float strength);
			void addMotionEvent( avs::InputId inputID, avs::vec2 motion);
		};
	}
}