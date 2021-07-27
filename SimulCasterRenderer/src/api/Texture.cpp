#include "Texture.h"
using namespace scr;


void Texture::TextureCreateInfo::Free()
{
	for(auto m:mips)
	{
		delete [] m;
	}
	mips.clear();
	mipSizes.clear();
}
const avs::vec3 Texture::DUMMY_DIMENSIONS = {1, 1, 1};

