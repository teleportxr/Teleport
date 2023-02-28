#pragma once

#include <cstring>	// for memcpy
#include <cstdint>
#include <vector>

namespace avs
{
	typedef unsigned long long uid;

	//One identifier per extension class.
	enum class MaterialExtensionIdentifier: uint32_t
	{
		SIMPLE_GRASS_WIND
	};

	//Abstract material extension class.
	class MaterialExtension
	{
	public:
	    virtual ~MaterialExtension() = default;
		virtual MaterialExtensionIdentifier getID() = 0;
		
		//Copies the data from the extension into a buffer.
		//	buffer : Buffer we want to copy the data into.
		virtual void serialise(std::vector<char> &buffer) = 0;
		//Copies the data from a buffer into the extension instance.
		//	buffer : Buffer to copy the data from.
		//	bufferOffset : Location to start copying from the buffer.
		virtual void deserialise(std::vector<uint8_t> &buffer, size_t &bufferOffset) = 0;
	};

	//Extension for holding the data on the SimpleGrassWind material function used in Unreal.
	class SimpleGrassWindExtension : public MaterialExtension
	{
	public:
		float windIntensity;
		float windWeight; ///Should also take a map for a full implementation.
		float windSpeed;

		uid texUID; ///(AdditionalWPO) Should be an arbitrary graph for a full implementation.

		SimpleGrassWindExtension()
			: SimpleGrassWindExtension(1.0f, 1.0f, 1.0f)
		{}

		SimpleGrassWindExtension(float windIntensity, float windWeight, float windSpeed, uid texUID = 0)
			: windIntensity(windIntensity), windWeight(windWeight), windSpeed(windSpeed), texUID(texUID)
		{}

		virtual MaterialExtensionIdentifier getID() override
		{
			return MaterialExtensionIdentifier::SIMPLE_GRASS_WIND;
		}

		virtual void serialise(std::vector<char> &buffer) override
		{
			size_t position = buffer.size();
			//Add size to store all of the extension's data.
			buffer.resize(position + sizeof(MaterialExtensionIdentifier) + sizeof(float) * 3 + sizeof(uid));

			MaterialExtensionIdentifier id = getID();
			memcpy(buffer.data() + position, &id, sizeof(MaterialExtensionIdentifier));
			position += sizeof(MaterialExtensionIdentifier);

			memcpy(buffer.data() + position, &windIntensity, sizeof(float));
			position += sizeof(float);

			memcpy(buffer.data() + position, &windWeight, sizeof(float));
			position += sizeof(float);

			memcpy(buffer.data() + position, &windSpeed, sizeof(float));
			position += sizeof(float);

			memcpy(buffer.data() + position, &texUID, sizeof(uid));
		}

		virtual void deserialise(std::vector<uint8_t> &buffer, size_t &bufferOffset) override
		{
			memcpy(&windIntensity, buffer.data() + bufferOffset, sizeof(float));
			bufferOffset += sizeof(float);

			memcpy(&windWeight, buffer.data() + bufferOffset, sizeof(float));
			bufferOffset += sizeof(float);

			memcpy(&windSpeed, buffer.data() + bufferOffset, sizeof(float));
			bufferOffset += sizeof(float);

			memcpy(&texUID, buffer.data() + bufferOffset, sizeof(uid));
			bufferOffset += sizeof(uid);
		}
	};
}