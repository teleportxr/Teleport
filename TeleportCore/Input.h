// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <map>
#include <stdint.h>
#include <vector>

#include "libavstream/common_input.h"

namespace teleport
{
	namespace core
	{
		//! A class to store current input states.
		class Input
		{
		public:			
			std::vector<avs::InputEventBinary> binaryEvents;
			std::vector<avs::InputEventAnalogue> analogueEvents;
			std::vector<avs::InputEventMotion> motionEvents;
			
			std::vector<uint8_t> binaryStates;
			std::vector<float> analogueStates;

			//! Clear the current lists of states and events.
			void clearEvents();
			//! Add a binary (on/off) event for the specified inputID.
			void addBinaryEvent( avs::InputId inputID, bool activated);
			//! Add an analogue [0,1.0] or [-1.0,1.0] event for the specified inputID.
			void addAnalogueEvent( avs::InputId inputID, float strength);
			//! Add a notion (xy) event for the specified inputID.
			void addMotionEvent( avs::InputId inputID, vec2 motion);
			//! Set a binary state for the specified inputID.
			void setBinaryState(uint16_t inputID, bool activated);
			//! Set an analogue state for the specified inputID.
			void setAnalogueState(uint16_t inputID, float strength);
		};
	}
}