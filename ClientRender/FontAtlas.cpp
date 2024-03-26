#include "ClientRender/FontAtlas.h"

using namespace teleport;
using namespace clientrender;


void FontAtlas::Save(std::ostream &o) const
{
	o<<(*this);
}