#include "ClientRender/FontAtlas.h"

using namespace teleport;
using namespace clientrender;


void FontAtlas::Save(std::ostream &o) const
{
	o<<(*this);
}

FontAtlas::FontAtlas(avs::uid u , const std::string &url) : teleport::core::FontAtlas(u), IncompleteFontAtlas(u, url)
{
}