// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <map>
#include <stdint.h>
#include <vector>

#include "TeleportCore/InputTypes.h"

namespace teleport
{
	namespace core
	{
		//! A class to store current input states.
		class Input
		{
			//! These are NOT sent to the server, but record the last event for each InputId.
			mutable std::vector<InputEventBinary> lastBinaryEvents;
			mutable std::vector<InputEventAnalogue> lastAnalogueEvents;
		public:	
			std::vector<InputEventBinary> binaryEvents;
			std::vector<InputEventAnalogue> analogueEvents;
			std::vector<InputEventMotion> motionEvents;
			
			std::vector<uint8_t> binaryStates;
			std::vector<float> analogueStates;

			//! Clear the current lists of states and events.
			void clearEvents();
			//! Add a binary (on/off) event for the specified inputID.
			void addBinaryEvent( InputId inputID, bool activated);
			//! Add an analogue [0,1.0] or [-1.0,1.0] event for the specified inputID.
			void addAnalogueEvent( InputId inputID, float strength);
			//! Add a notion (xy) event for the specified inputID.
			void addMotionEvent( InputId inputID, vec2 motion);
			//! Set a binary state for the specified inputID.
			void setBinaryState(uint16_t inputID, bool activated);
			//! Set an analogue state for the specified inputID.
			void setAnalogueState(uint16_t inputID, float strength);

			//! For debugging only: get the last event at this id.
			const InputEventAnalogue& getLastAnalogueEvent(uint16_t inputID) const;
			//! For debugging only: get the last event at this id.
			const InputEventBinary& getLastBinaryEvent(uint16_t inputID) const;
		};
	}
}