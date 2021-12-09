#pragma once

#include <cstdint>

#include "common.hpp"
#include "common_maths.h"

namespace avs
{
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
	//List of all IDs for all input types; list contains common aliases.
	enum class InputId
	{
		INVALID,

		BUTTON01,
		BUTTON02,
		BUTTON03,
		BUTTON04,
		BUTTON05,
		BUTTON06,
		BUTTON07,
		BUTTON08,
		BUTTON09,
		BUTTON10,

		TRIGGER01,
		TRIGGER02,
		TRIGGER03,
		TRIGGER04,
		TRIGGER05,
		TRIGGER06,
		TRIGGER07,
		TRIGGER08,
		TRIGGER09,
		TRIGGER10,

		MOTION01,
		MOTION02,
		MOTION03,
		MOTION04,
		MOTION05,
		MOTION06,
		MOTION07,
		MOTION08,
		MOTION09,
		MOTION10,

		//Button aliases.
		BUTTON_A = BUTTON01,
		BUTTON_B = BUTTON02,
		BUTTON_X = BUTTON03,
		BUTTON_Y = BUTTON04,

		BUTTON_LEFT_STICK = BUTTON05,
		BUTTON_RIGHT_STICK = BUTTON06,

		BUTTON_HOME = BUTTON07,

		//Single stick alias.
		BUTTON_STICK = BUTTON_LEFT_STICK,

		//Trigger aliases.
		TRIGGER_LEFT_BACK = TRIGGER01,
		TRIGGER_RIGHT_BACK = TRIGGER02,
		TRIGGER_LEFT_FRONT = TRIGGER03,
		TRIGGER_RIGHT_FRONT = TRIGGER04,
		TRIGGER_LEFT_GRIP = TRIGGER05,
		TRIGGER_RIGHT_GRIP = TRIGGER06,

		//Single trigger aliases.
		TRIGGER_BACK = TRIGGER_LEFT_BACK,
		TRIGGER_FRONT = TRIGGER_LEFT_FRONT,
		TRIGGER_GRIP = TRIGGER_LEFT_GRIP,

		//Motion aliases.
		STICK_LEFT = MOTION01,
		STICK_RIGHT = MOTION02,

		TRACKPAD_LEFT = MOTION03,
		TRACKPAD_RIGHT = MOTION04,

		//Single motion aliases.
		STICK = STICK_LEFT,
		TRACKPAD = TRACKPAD_LEFT,
	};

	//Input events that can only be in two states; e.g. button pressed or not.
	struct InputEventBinary
	{
		uint32_t eventID = 0;
		InputId inputID = InputId::INVALID; //ID of the input type used that triggered the event.
		bool activated = false;
	} AVS_PACKED;

	//Input events that can be normalised between two values; e.g. how pressed a trigger is.
	struct InputEventAnalogue
	{
		uint32_t eventID = 0;
		InputId inputID = InputId::INVALID; //ID of the input type used that triggered the event.
		float strength = 0.0f;

		//Set the value normalised between 0 and 1.
		//	value : The raw strength before normalisation.
		//	maxValue : The max value the data can be from the source; i.e. fully pressed.
		void setNormalised(float value, float maxValue)
		{
			strength = value / maxValue;
		}
	} AVS_PACKED;

	//Input events that represent the motion in two directions; e.g. a stick on a controller.
	struct InputEventMotion
	{
		uint32_t eventID = 0;
		InputId inputID = InputId::INVALID; //ID of the input type used that triggered the event.
		vec2 motion = vec2{0.0f, 0.0f};
	} AVS_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif
} //namespace avs
