#include "Identity.h"
// Google OAuth relies on curl/openssl and a local HTTP redirect server,
// which are not available in the Android client build.
#if !defined(PLATFORM_ANDROID)
#include "GoogleOAuth.h"
#endif

using namespace teleport;
using namespace client;

Identity teleport::client::identity;

void Identity::Init()
{
	identity = "";
#if !defined(PLATFORM_ANDROID)
	if(GoogleOAuthPKCE::TryAuthenticate())
		Identity::identity = GoogleOAuthPKCE::GetIdentity();
#endif
}