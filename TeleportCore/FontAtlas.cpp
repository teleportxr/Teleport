#include "FontAtlas.h"
#include "TeleportCore/Logging.h"
#include <nlohmann/json.hpp>

using namespace teleport;
using namespace core;

FontAtlas::FontAtlas(avs::uid u)
{
	uid = u;
}

void FontAtlas::ToJson(std::string &json) const
{
	nlohmann::json j;
	j["font_texture_path"] = font_texture_path;
	for (const auto &f : fontMaps)
	{
		nlohmann::json fontMapJson;
		fontMapJson["lineHeight"] = f.second.lineHeight;
		for (const auto &g : f.second.glyphs)
		{
			nlohmann::json glyphJson;
			glyphJson["index"]		  = g.index;
			glyphJson["x0"]		  = g.x0;
			glyphJson["y0"]		  = g.y0;
			glyphJson["x1"]		  = g.x1;
			glyphJson["y1"]		  = g.y1;
			glyphJson["xOffset"]  = g.xOffset;
			glyphJson["yOffset"]  = g.yOffset;
			glyphJson["xAdvance"] = g.xAdvance;
			glyphJson["xOffset2"] = g.xOffset2;
			glyphJson["yOffset2"] = g.yOffset2;
			fontMapJson["glyphs"].push_back(glyphJson);
		}
		std::string mapName	   = std::to_string(f.first);
		j["fontMaps"][mapName]=fontMapJson;
	}
	json = j.dump(1,'\t');
}

void FontAtlas::FromJson(const std::string &json)
{
	try
	{
		nlohmann::json j  = nlohmann::json::parse(json);
		font_texture_path = j["font_texture_path"];
		fontMaps.clear();
		auto jm=j["fontMaps"];
		for (auto f:jm.items())
		{
			FontMap &fontMap=fontMaps[std::atoi(f.key().c_str())];
			const auto &f2		= f.value();
			fontMap.lineHeight = f2["lineHeight"];
			for (const auto &g : f2["glyphs"])
			{
				Glyph glyph;
				glyph.index	   = g["index"];
				glyph.x0	   = g["x0"];
				glyph.y0	   = g["y0"];
				glyph.x1	   = g["x1"];
				glyph.y1	   = g["y1"];
				glyph.xOffset  = g["xOffset"];
				glyph.yOffset  = g["yOffset"];
				glyph.xAdvance = g["xAdvance"];
				glyph.xOffset2 = g["xOffset2"];
				glyph.yOffset2 = g["yOffset2"];
				fontMap.glyphs.push_back(glyph);
			}
		}
	}
	catch (const std::exception &e)
	{
		TELEPORT_WARN("Failed to parse JSON: {}", e.what());
	}
}