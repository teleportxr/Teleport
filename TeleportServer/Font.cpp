
#include "Font.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h> // optional, used for better bitmap packing
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <fmt/core.h>

#include <TeleportCore/ErrorHandling.h>
#include <TeleportCore/Logging.h>
#include <fstream>
#include <filesystem>
#include "libavstream/geometry/mesh_interface.hpp"
#include <GeometryStore.h>

#include <msdf-atlas-gen/msdf-atlas-gen.h>

using namespace msdf_atlas;

using namespace teleport;
using namespace server;


bool server::Font::ExtractMsdfFont(core::FontAtlas &fontAtlas,std::string ttf_path_utf8, std::string generate_texture_path_utf8, std::string atlas_chars,avs::Texture &avsTexture
	,std::vector<int> sizes)
{
	bool success = false;
	// Initialize instance of FreeType library
	msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
	if(!ft)
		return false;
	
	// Load font file
	msdfgen::FontHandle *font = msdfgen::loadFont(ft, ttf_path_utf8.c_str());
	if(!font)
		return false;
	
	// Storage for glyph geometry and their coordinates in the atlas
	std::vector<GlyphGeometry> glyphs;
	// FontGeometry is a helper class that loads a set of glyphs from a single font.
	// It can also be used to get additional font metrics, kerning information, etc.
	FontGeometry fontGeometry(&glyphs);
	// Load a set of character glyphs:
	// The second argument can be ignored unless you mix different font sizes in one atlas.
	// In the last argument, you can specify a charset other than ASCII.
	// To load specific glyph indices, use loadGlyphs instead.
	//fontGeometry.loadCharset(font, 1.0, Charset::ASCII);
	Charset charset;
	
	std::string chars=" !\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
	std::string charset_input="";
	for(size_t i=0;i<chars.size();i++)
		charset_input+=fmt::format("{} ",(int)chars[i]);
	charset.parse(charset_input.c_str(),charset_input.size());
	//fontGeometry.loadGlyphset(font,1.0,charset);
	fontGeometry.loadCharset(font,1.0,Charset::ASCII);
	// Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
	const double maxCornerAngle = 3.0;
	for (GlyphGeometry &glyph : glyphs)
		glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
	// TightAtlasPacker class computes the layout of the atlas.
	TightAtlasPacker packer;
	// Set atlas parameters:
	// setDimensions or setDimensionsConstraint to find the best value
	packer.setDimensionsConstraint(DimensionsConstraint::POWER_OF_TWO_RECTANGLE);
	// setScale for a fixed size or setMinimumScale to use the largest that fits
	packer.setMinimumScale(24.0);
	// setPixelRange or setUnitRange
	packer.setPixelRange(4.0);
	packer.setMiterLimit(1.0);
	// Compute atlas layout - pack glyphs
	packer.pack(glyphs.data(), (int)glyphs.size());
	// Get final atlas dimensions
	int width = 0, height = 0;
	packer.getDimensions(width, height);
	// The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
	ImmediateAtlasGenerator<float,						// pixel type of buffer for individual glyphs depends on generator function
							3,							// number of atlas color channels
							msdfGenerator,				// function to generate bitmaps for individual glyphs
							BitmapAtlasStorage<byte, 3> // class that stores the atlas bitmap
							// For example, a custom atlas storage class that stores it in VRAM can be used.
							>
		generator(width, height);
	// GeneratorAttributes can be modified to change the generator's default settings.
	GeneratorAttributes attributes;
	generator.setAttributes(attributes);
	generator.setThreadCount(4);
	// Generate atlas bitmap
	generator.generate(glyphs.data(), (int)glyphs.size());
	// The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
	const BitmapAtlasStorage<byte,3> &atlas=generator.atlasStorage();
	msdfgen::Bitmap<byte,3> bitmap=atlas.operator msdfgen::BitmapConstRef<unsigned char, 3>();
	
	using namespace std;
	filesystem::path ttf_path(ttf_path_utf8.c_str());
	avsTexture.name = (const char*)ttf_path.filename().generic_u8string().c_str();
	avsTexture.width=bitmap.width();
	avsTexture.height=bitmap.height();
	avsTexture.depth=1;
	avsTexture.arrayCount=1;
	avsTexture.mipCount=1;
	avsTexture.format=avs::TextureFormat::RGBA8;
	avsTexture.compression=avs::TextureCompression::PNG;
	avsTexture.compressed=false;
	// now cropped:
	uint32_t imageSize	  = height * width * 4;
	avsTexture.images.resize(1);
	avsTexture.images[0].data.resize(imageSize);
	auto &img=avsTexture.images[0];
	uint8_t *target = avsTexture.images[0].data.data();
	//bool black = true;
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			size_t n		= (i * width + j) * 4;
			uint8_t  *b		= bitmap(j,height-1-i);
			img.data[n + 0] = b[0];
			img.data[n + 1] = b[1];
			img.data[n + 2] = b[2];
			img.data[n + 3] = 255;
		}
	}


	// The glyphs array (or fontGeometry) contains positioning data for typesetting text.
    // calculate fill rate and crop the bitmap
    int filled = 0;
	auto &fontMap = fontAtlas.fontMaps[128];
	fontMap.lineHeight=100;
	float sc=fontMap.lineHeight;
	fontMap.glyphs.resize(glyphs.size());
	for (int i = 0; i < glyphs.size(); i++)
	{
		GlyphGeometry &g = glyphs[i];
		auto &G=fontMap.glyphs[i];
		//const auto &rect = g.getBoxRect();
		double x0=0, y0=0, x1=0, y1=0;
		g.getQuadPlaneBounds(x0,y0,x1,y1);
		double texc_x0=0, texc_y0=0, texc_x1=0, texc_y1=0;
		g.getQuadAtlasBounds(texc_x0,texc_y0,texc_x1,texc_y1);
		double xAdvance = g.getAdvance();
		unicode_t char_id=g.getCodepoint();
		size_t pos=chars.find(char_id);
		if(pos>=charset_input.length())
			pos = 0xFFFF;
		G.index			= (uint16_t)pos;

		G.x0	   = uint16_t(texc_x0);				//rect.x;
		G.y1	   = uint16_t(height - texc_y0);	//height - rect.y;
		G.x1	   = uint16_t(texc_x1);				//rect.x + rect.w;
		G.y0	   = uint16_t(height - texc_y1);	//height - (rect.y + rect.h);

		G.xOffset  = (float)x0*sc;
		G.yOffset  = (float)(1.0-y1)*sc;
		G.xAdvance = (float)xAdvance*sc;
		G.xOffset2 = (float)x1*sc;
		G.yOffset2 = (float)(1.0-y0)*sc;
	}
	// Cleanup
	msdfgen::destroyFont(font);
	
	msdfgen::deinitializeFreetype(ft);
	
	return true;
}

bool server::Font::ExtractFont(core::FontAtlas &fontAtlas,std::string ttf_path_utf8, std::string generate_texture_path_utf8, std::string atlas_chars,avs::Texture &avsTexture
	,std::vector<int> sizes)
{
	fontAtlas.font_texture_path=generate_texture_path_utf8;
	using namespace std;
	size_t numSizes=sizes.size();
    /* load font file */
    unsigned char* fontBuffer=nullptr;
    
	{
		long fsize=0;
		ifstream loadFile(ttf_path_utf8, std::ios::binary);
		if(!loadFile.good())
			return 0;
		loadFile.seekg(0,ios::end);
        fsize = (long)loadFile.tellg();
		loadFile.seekg(0,ios::beg);
		fontBuffer = new unsigned char[fsize];
		loadFile.read((char*)fontBuffer,fsize);
	}
	#define NUM_GLYPHS 95
	
    // setup glyph info stuff, check stb_truetype.h for definition of structs
	vector<vector<stbtt_packedchar>> glyph_metrics(numSizes);
	const int first_char=32;
    vector<stbtt_pack_range> ranges(numSizes);
	for(size_t i=0;i<numSizes;i++)
	{
		auto &fontMap=fontAtlas.fontMaps[sizes[i]];
		fontMap.glyphs.resize(NUM_GLYPHS);
		glyph_metrics[i].resize(NUM_GLYPHS);
		stbtt_pack_range &range					=ranges[i];
		range.font_size							=float(sizes[i]);
		range.first_unicode_codepoint_in_range	=first_char;
		range.array_of_unicode_codepoints		=NULL;
		range.num_chars							=NUM_GLYPHS;
		range.chardata_for_range				=glyph_metrics[i].data();
		range.h_oversample						=0;
		range.v_oversample						=0;
	}
    // make a most likely large enough bitmap, adjust to font type, number of sizes and glyphs and oversampling
    int width = 1024;
    int max_height = 1024;
	uint32_t mip0_bytesize=max_height *width;
    unsigned char* bitmap = new unsigned char[mip0_bytesize];
    // do the packing, based on the ranges specified
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, bitmap, width, max_height, 0, 1, NULL);   
    stbtt_PackSetOversampling(&pc, 1, 1); // say, choose 3x1 oversampling for subpixel positioning
    stbtt_PackFontRanges(&pc, fontBuffer, 0, ranges.data(), (int)ranges.size());
    stbtt_PackEnd(&pc);

    // get the global metrics for each size/range
    stbtt_fontinfo info;
    stbtt_InitFont(&info, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer,0));

    vector<float> ascents(numSizes);
    vector<float> descents(numSizes);
    vector<float> linegaps(numSizes);

    for (int i = 0; i <(int) numSizes; i++)
	{
    	float size = ranges[i].font_size;
        float scale = stbtt_ScaleForPixelHeight(&info, size);
        int a, d, l;
        stbtt_GetFontVMetrics(&info, &a, &d, &l);
        
        ascents[i]  = a*scale;
        descents[i] = d*scale;
        linegaps[i] = l*scale;
		
		auto &fontMap=fontAtlas.fontMaps[sizes[i]];
		fontMap.lineHeight=ascents[i]- descents[i] + linegaps[i];
    }

    // calculate fill rate and crop the bitmap
    int filled = 0;
    int height = 0;
    for (int i = 0; i <(int) numSizes; i++)
	{
		auto &fontMap=fontAtlas.fontMaps[sizes[i]];
		
        for (int j = 0; j < NUM_GLYPHS; j++)
		{
			stbtt_packedchar &metric=glyph_metrics[i][j];
            if (metric.y1 > height)
				height = metric.y1;
            filled += (metric.x1 - metric.x0)*(metric.y1 - metric.y0);
			
			fontMap.glyphs[j].x0		=metric.x0;
			fontMap.glyphs[j].y0		=metric.y0;
			fontMap.glyphs[j].x1		=metric.x1;
			fontMap.glyphs[j].y1		=metric.y1;
			fontMap.glyphs[j].xOffset	=metric.xoff;
			fontMap.glyphs[j].yOffset	=metric.yoff;
			fontMap.glyphs[j].xAdvance	=metric.xadvance;
			fontMap.glyphs[j].xOffset2	=metric.xoff2;
			fontMap.glyphs[j].yOffset2	=metric.yoff2;
        }
    }
	filesystem::path ttf_path(ttf_path_utf8.c_str());
	avsTexture.name = (const char*)ttf_path.filename().generic_u8string().c_str();
	avsTexture.width=width;
	avsTexture.height=height;
	avsTexture.depth=1;
	avsTexture.arrayCount=1;
	avsTexture.mipCount=1;
	avsTexture.format=avs::TextureFormat::G8;
	avsTexture.compression=avs::TextureCompression::KTX;
	avsTexture.compressed=false;

	// now cropped:
	uint32_t imageSize	  = height * width;
	uint16_t numImages	  = 1;
	uint32_t offset0	  = uint32_t(sizeof(numImages) + sizeof(imageSize));
	avsTexture.images.resize(numImages);
	avsTexture.images[0].data.resize(imageSize);
	uint8_t *target = avsTexture.images[0].data.data();
	memcpy(target, bitmap, imageSize);
	bool black = true;
	for (size_t i = 0; i < mip0_bytesize; i++)
	{
		if (target[i] != 0)
		{
			black=false;
			break;
		}
	}
	if(black)
	{
		TELEPORT_WARN("Black font texture {0}",avsTexture.name);
	}
    // create file
    TELEPORT_INTERNAL_COUT("height = {0}, fill rate = {1}\n", height, 100*filled/(double)(width*height)); fflush(stdout);
	//std::string png_filename=std::string(ttf_path_utf8)+".png";


	//void write_data(void *context, void *data, int size);
	//int res=stbi_write_png_to_func(stbi_write_func *func, this, int x, int y, int comp, const void *data, int stride_bytes)

    // print info
    if (0)
	{
        for (int j = 0; j < numSizes; j++)
		{
            TELEPORT_INTERNAL_COUT("size    {0}\n", ranges[j].font_size);
            TELEPORT_INTERNAL_COUT("ascent  {0}\n", ascents[j]);
            TELEPORT_INTERNAL_COUT("descent {0}\n", descents[j]);
            TELEPORT_INTERNAL_COUT("linegap {0}\n", linegaps[j]);
            vector<stbtt_packedchar> m = glyph_metrics[j];
			 for (int i = 0; i < NUM_GLYPHS; i++)
			{
                TELEPORT_INTERNAL_COUT("    '{0}':  (x0,y0) = ({1},{2}),  (x1,y1) = ({3},{4}),  (xoff,yoff) = ({5},{6}),  (xoff2,yoff2) = ({7},{8}),  xadvance = {9}\n", 
                       32+i, m[i].x0, m[i].y0, m[i].x1, m[i].y1, m[i].xoff, m[i].yoff, m[i].xoff2, m[i].yoff2, m[i].xadvance);
            }
        }   
    }

    delete[] fontBuffer;
    delete[] bitmap;
    
    return true;
}
	
void server::Font::Free(avs::Texture &avsTexture)
{
	avsTexture.images.clear();
}

server::Font &server::Font::GetInstance()
{
	static Font font;
	return font;
}

server::Font::~Font()
{
	for(auto a:interopFontAtlases)
	{
		for(int i=0;i<a.second.numMaps;i++)
		{
			InteropFontMap &fontMap=a.second.fontMaps[i];
			delete [] fontMap.fontGlyphs;
		}
		delete [] a.second.fontMaps;
	}
	interopFontAtlases.clear();
}

bool server::Font::GetInteropFontAtlas(std::string path,InteropFontAtlas *interopFontAtlas)
{
	avs::uid uid=GeometryStore::GetInstance().PathToUid(path);
	if(!uid)
		return false;
	InteropFontAtlas *sourceAtlas=nullptr;
	const auto &f=interopFontAtlases.find(path);
	if(f==interopFontAtlases.end())
	{
		sourceAtlas=&(interopFontAtlases[path]);
		const core::FontAtlas *fontAtlas=GeometryStore::GetInstance().getFontAtlas(uid);
		sourceAtlas->numMaps=(int)fontAtlas->fontMaps.size();
		sourceAtlas->fontMaps=new InteropFontMap[sourceAtlas->numMaps];
		int i=0;
		for(auto m:fontAtlas->fontMaps)
		{
			sourceAtlas->fontMaps[i].size=m.first;
			sourceAtlas->fontMaps[i].numGlyphs=(int)m.second.glyphs.size();
			sourceAtlas->fontMaps[i].fontGlyphs=new core::Glyph[m.second.glyphs.size()];
			memcpy(sourceAtlas->fontMaps[i].fontGlyphs,m.second.glyphs.data(),sizeof(core::Glyph)*m.second.glyphs.size());
			i++;
		}
	}
	sourceAtlas=&(interopFontAtlases[path]);
	interopFontAtlas->fontMaps=sourceAtlas->fontMaps;
	return true;
}