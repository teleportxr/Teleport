#pragma once

#include <cstdint>

namespace teleport
{
	namespace client
	{
		class ServerTimestamp
		{
		public:
			ServerTimestamp() = delete;

			//Sets the last received timestamp from the server.
			//	value : Timestamp received from the server; should be a UTC Unix timestamp in milliseconds.
			static void setLastReceivedTimestamp(uint64_t value);

			//Returns the current timestamp.
			static double getCurrentTimestamp();

			//Tick current timestamp along.
			//	deltaTime : Amount of time that has passed since the last process tick.
			static void tick(double deltaTime);

		private:
			static uint64_t lastReceivedTimestamp; //Last timestamp received from the server; should be a UTC Unix timestamp in milliseconds.
			static double currentTimestamp; //Current time on the client in relation to the server.
		};
	}
}