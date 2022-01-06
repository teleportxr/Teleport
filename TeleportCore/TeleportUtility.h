#pragma once

#include <random>

class TeleportUtility








{
public:
	static uint32_t GenerateID()
	{
		std::random_device rd;  //Will be used to obtain a seed for the random number engine
		std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
		std::uniform_int_distribution<> dis(1);

		return static_cast<uint32_t>(dis(gen));
	}
};