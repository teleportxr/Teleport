#pragma once

#include <cstdint>

#include "common.hpp"
#include "common_maths.h"

#ifndef AVS_PACKED
	#if defined(__GNUC__) || defined(__clang__)
		#define AVS_PACKED __attribute__ ((packed,aligned(1)))
	#else
		#def ine AVS_PACKED
	#endif
#endif

namespace avs
{
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
	//! An input identifier, used between client and server to denote a specific input.
	typedef uint16_t InputId;
	//! What type of input to send, and when. 
	enum class InputType : uint8_t
	{
		Invalid=0,
		IsEvent=1,
		IsReleaseEvent=2,	// For use only with IsEvent
		IsInteger=4,
		IsFloat=8,
		IntegerState = IsInteger,
		FloatState = IsFloat,
		IntegerEvent = IsInteger | IsEvent,
		ReleaseEvent = IsInteger | IsEvent | IsReleaseEvent,
		FloatEvent = IsFloat | IsEvent
	};
	inline const char* stringof(InputType t)
	{
		switch (t)
		{
		case InputType::IsEvent			:return "Incomplete";
		case InputType::IsReleaseEvent	:return "Incomplete";
		case InputType::IntegerState 	:return "IntegerState";
		case InputType::FloatState		:return "FloatState";
		case InputType::IntegerEvent	:return "IntegerEvent";
		case InputType::ReleaseEvent	:return "ReleaseEvent";
		case InputType::FloatEvent		:return "FloatEvent";
		case InputType::Invalid:
		default:
			return "Invalid";
		};
	}
	inline InputType operator|(const InputType& a, const InputType& b)
	{
		return (InputType)((uint8_t)a | (uint8_t)b);
	}

	inline InputType operator&(const InputType& a, const InputType& b)
	{
		return (InputType)((uint8_t)a & (uint8_t)b);
	}

	//! Input events that can only be in two states; e.g. button pressed or not.
	struct InputEventBinary
	{
		uint32_t eventID = 0;
		InputId inputID = 0; //ID of the input type used that triggered the event.
		bool activated = false;
	} AVS_PACKED;

	//! Input events that can be normalised between two values; e.g. how pressed a trigger is.
	struct InputEventAnalogue
	{
		uint32_t eventID = 0;
		InputId inputID = 0; //ID of the input type used that triggered the event.
		float strength = 0.0f;

		//Set the value normalised between 0 and 1.
		//	value : The raw strength before normalisation.
		//	maxValue : The max value the data can be from the source; i.e. fully pressed.
		void setNormalised(float value, float maxValue)
		{
			strength = value / maxValue;
		}
	} AVS_PACKED;

	//! Input events that represent the motion in two directions; e.g. a stick on a controller.
	struct InputEventMotion
	{
		uint32_t eventID = 0;
		InputId inputID = 0; // ID of the input type used that triggered the event.
		vec2 motion = vec2{0.0f, 0.0f};
	} AVS_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif
} //namespace avs
