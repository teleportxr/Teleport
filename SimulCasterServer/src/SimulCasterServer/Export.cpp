#include "SimulCasterServer/Export.h"
#include "SimulCasterServer/CasterContext.h"

using namespace SCServer;
using namespace avs;
static std::map<uid,CasterContext *> casterContexts;
bool Initialize()
{
	return true;
}
bool Uninitialize()
{
	return true;
}
bool CreateContext(uid u)
{
	if(casterContexts.find(u)!=casterContexts.end())
		return false;
	CasterContext *casterContext=new CasterContext();
	casterContexts[u]=casterContext;
}
bool DestroyContext(uid u)
{
	auto &c=casterContexts.find(u);
	if(c==casterContexts.end())
		return false;
	CasterContext *casterContext=c->second;
	if(!casterContext)
		return false;
	delete casterContext;
	casterContexts.erase(c);
	return true;
}