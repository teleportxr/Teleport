#pragma once

#include <cstdint>

namespace teleport
{
	namespace client
	{
		//! A static-only class to hold and manage server timestamps.
		class ServerTimestamp
		{
		public:
			ServerTimestamp() = delete;

			//Sets the last received timestamp from the server.
			//	value : Timestamp received from the server; should be a UTC Unix timestamp in milliseconds.
			static void setLastReceivedTimestampUTCUnixMs(uint64_t value);

			//Returns the current timestamp.
			static double getCurrentTimestampUTCUnixNs();
			static double getCurrentTimestampUTCUnixS();

			//Tick current timestamp along.
			//	deltaTime : Amount of time that has passed since the last process tick.
			static void tick(double deltaTime_seconds);

		private:
			static uint64_t lastReceivedTimestampUTCUnixNs; //Last timestamp received from the server; should be a UTC Unix timestamp in milliseconds.
			static double currentTimestampUTCUnixNs; //Current time on the client in relation to the server.
		};
	}
}