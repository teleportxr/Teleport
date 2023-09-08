
#include "Resource.h"
#include "TeleportCore/ErrorHandling.h"
using namespace clientrender;

void IncompleteResource::DecrementMissingResources()
{
	missingResourceCount--;
	if(missingResourceCount==0xFFFFFFFF)
	{
		DEBUG_BREAK_ONCE
	}
}
