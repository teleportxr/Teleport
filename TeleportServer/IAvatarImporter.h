#pragma once

namespace teleport
{
	namespace server
	{
		class IAvatarImporter
		{
		public:
			virtual avs::uid ImportValidatedForClient(avs::uid, const teleport::server::AvatarValidationResult &) = 0;
			virtual avs::uid ImportDefaultForClient(avs::uid) = 0;
			virtual void RemoveForClient(avs::uid) = 0;
		};
	}
}