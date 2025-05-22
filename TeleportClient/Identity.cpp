#include "Identity.h"
#include "GoogleOAuth.h"

using namespace teleport;
using namespace client;

Identity teleport::client::identity;

void Identity::Init()
{
	identity = "";
	if(GoogleOAuthPKCE::TryAuthenticate())
		Identity::identity = GoogleOAuthPKCE::GetIdentity();
}