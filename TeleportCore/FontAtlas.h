#pragma once
#include "libavstream/common.hpp"
#include "TeleportCore/CommonNetworking.h"
#include <stdint.h>
#include <vector>
#include <map>

namespace teleport
{
	namespace core
	{
#pragma pack(push)
#pragma pack(1)
		//! A mapping from a single character to its position in a font map texture.
		struct Glyph
		{
			uint16_t index = 0;
			uint16_t x0 = 0;
			uint16_t y0 = 0;
			uint16_t x1 = 0;
			uint16_t y1 = 0; // coordinates of bounding box in bitmap
			float xOffset = 0.0f;
			float yOffset = 0.0f;
			float xAdvance = 0.0f;
			float xOffset2 = 0.0f;
			float yOffset2 = 0.0f;
			bool Verify(const Glyph& g)
			{
				if (index != g.index)
					return false;
				if (x0 != g.x0)
					return false;
				if (y0 != g.y0)
					return false;
				if (x1 != g.x1)
					return false;
				if (y1 != g.y1)
					return false;
				if (xOffset != g.xOffset)
					return false;
				if (yOffset != g.yOffset)
					return false;
				if (xAdvance != g.xAdvance)
					return false;
				if (xOffset2 != g.xOffset2)
					return false;
				if (yOffset2 != g.yOffset2)
					return false;
				return true;
			}
		} TELEPORT_PACKED;
#pragma pack(pop)
		static_assert (sizeof(Glyph) == 30, "Glyph Size is not correct");

		//! A mapping from characters to their positions in the font map texture.
		struct FontMap
		{
			std::vector<Glyph> glyphs;
			float lineHeight = 0.0f;
			bool Verify(const FontMap& f)
			{
				if (lineHeight != f.lineHeight)
					return false;
				if (glyphs.size() != f.glyphs.size())
					return false;
				for (size_t i = 0; i < glyphs.size(); i++)
				{
					if (!glyphs[i].Verify(f.glyphs[i]))
						return false;
				}
				return true;
			}
		};

		//! Each font size represented has a FontMap.
		struct FontAtlas
		{
			FontAtlas(avs::uid u = 0);
			void ToJson(std::string &json) const;
			void FromJson(const std::string &json);
			avs::uid uid = 0;
			std::string name;
			std::string font_texture_path;
			// Not saved! path is the ground-truth.
			avs::uid font_texture_uid = 0;
			std::map<uint16_t, FontMap> fontMaps;
			bool Verify(const FontAtlas& t) const
			{
				if (font_texture_path != t.font_texture_path)
					return false;
				if (fontMaps.size() != t.fontMaps.size())
					return false;
				for (auto f : fontMaps)
				{
					auto u = t.fontMaps.find(f.first);
					if (u == t.fontMaps.end())
						return false;
					if (!f.second.Verify(u->second))
						return false;
				}
				return true;
			}
		};

		template<typename OutStream>
		OutStream& operator<< (OutStream& out, const Glyph& g)
		{
			out.write((const char*)&g, sizeof(g));
			return out;
		}

		template<typename InStream>
		InStream& operator>> (InStream& in, Glyph& g)
		{
			in.read((char*)&g, sizeof(g));
			return in;
		}
		template<typename OutStream>
		OutStream& operator<< (OutStream& out, const FontMap& fontMap)
		{
			out.write((char*)&fontMap.lineHeight, sizeof(fontMap.lineHeight));
			size_t sz = fontMap.glyphs.size();
			out.write((char*)&sz, sizeof(sz));
			for (auto f : fontMap.glyphs)
			{
				out << f;
			}
			return out;
		}

		template<typename InStream>
		InStream& operator>> (InStream& in, FontMap& fontMap)
		{
			in.read((char*)&fontMap.lineHeight, sizeof(fontMap.lineHeight));
			size_t sz;
			in.read((char*)&sz, sizeof(sz));
			if (sz > 1000)
				return in;
			fontMap.glyphs.resize(sz);
			for (size_t i = 0; i < sz; i++)
			{
				in >> fontMap.glyphs[i];
			}
			return in;
		}
		template<typename OutStream>
		OutStream& operator<< (OutStream& out, const FontAtlas& fontAtlas)
		{
			size_t sz = fontAtlas.font_texture_path.size();
			out.write((char*)&sz, sizeof(sz));
			out.write((char*)fontAtlas.font_texture_path.data(), sz);
			sz = fontAtlas.fontMaps.size();
			out.write((char*)&sz, sizeof(sz));
			for (auto f : fontAtlas.fontMaps)
			{
				out.write((char*)&f.first, sizeof(f.first));
				out << f.second;
			}
			return out;
		}

		template<typename InStream>
		InStream& operator>> (InStream& in, FontAtlas& fontAtlas)
		{
			size_t sz = 0;
			in.read((char*)&sz, sizeof(sz));
			fontAtlas.font_texture_path.resize(sz);
			in.read((char*)fontAtlas.font_texture_path.data(), sz);
			in.read((char*)&sz, sizeof(sz));
			for (size_t i = 0; i < sz; i++)
			{
				int fontSize = 0;
				in.read((char*)&fontSize, sizeof(fontSize));
				in >> fontAtlas.fontMaps[fontSize];
			}
			return in;
		}
	}
}