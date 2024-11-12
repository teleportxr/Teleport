// (C) Copyright 2018-2024 Simul Software Ltd
#include "ClientRender/CanvasTextRenderer.h"
#include "ClientRender/TextCanvas.h"
#include "ClientRender/Texture.h"
#include "ClientRender/FontAtlas.h"
#include "Platform/CrossPlatform/Macros.h"

using namespace teleport;
using namespace clientrender;

using namespace teleport;
using namespace clientrender;
using namespace platform;
using namespace crossplatform;

CanvasTextRenderer::CanvasTextRenderer()
{
}

CanvasTextRenderer::~CanvasTextRenderer()
{
	InvalidateDeviceObjects();
}

void CanvasTextRenderer::RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r)
{
	reload_shaders = true;
	renderPlatform = r;
	textConstants.RestoreDeviceObjects(r);
}

void CanvasTextRenderer::InvalidateDeviceObjects()
{
	effect.reset();
	reload_shaders = true;
	textConstants.InvalidateDeviceObjects();
	renderPlatform=nullptr;
}

void CanvasTextRenderer::RecompileShaders()
{
	if(renderPlatform)
		renderPlatform->ScheduleRecompileEffects({"canvas_text"}, [this]()
											 { reload_shaders = true; });
}

void CanvasTextRenderer::Render(platform::crossplatform::GraphicsDeviceContext &deviceContext, const CanvasRender *canvasRender)
{
	if(!effect)
		return;
	const auto &textCanvasCreateInfo = canvasRender->textCanvas->textCanvasCreateInfo;
	const vec4 no_background={0,0,0,0};
	Render(deviceContext, canvasRender->textCanvas->fontAtlas.get(), textCanvasCreateInfo.size, textCanvasCreateInfo.text, textCanvasCreateInfo.colour, 
		{-textCanvasCreateInfo.width / 2.0f, textCanvasCreateInfo.height / 2.0f, textCanvasCreateInfo.width, -textCanvasCreateInfo.height}, no_background, textCanvasCreateInfo.lineHeight, canvasRender->textCanvas->fontChars);
}

void CanvasTextRenderer::Render(platform::crossplatform::GraphicsDeviceContext &deviceContext, const clientrender::FontAtlas *fontAtlas
									,int size
									,const std::string &text
									,vec4 colour, vec4 canvas, vec4 bkg
									,float lineHeight
									,platform::crossplatform::StructuredBuffer<FontChar> &fontChars
									,bool link)
{
bool centre=link;
	if (!effect || reload_shaders)
	{
		effect.reset();
		effect = renderPlatform->GetOrCreateEffect("canvas_text");
		tech = effect->GetTechniqueByName("text");
		link_tech = effect->GetTechniqueByName("link_text");
		auto bkgTech = effect->GetTechniqueByName("background");
		if(tech)
		{
			singleViewPass = tech->GetPass("singleview");
			multiViewPass = tech->GetPass("multiview");
			backgroundPass = bkgTech->GetPass("singleview");
			backgroundPassMV = bkgTech->GetPass("multiview");
			
		}
		if(link_tech)
		{
			link_singleViewPass = link_tech->GetPass("singleview");
			link_multiViewPass = link_tech->GetPass("multiview");
		}
		textureResource = effect->GetShaderResource("fontTexture");
		_fontChars = effect->GetShaderResource("fontChars");
		reload_shaders = false;
	}
	if (!effect)
		return;
	if (!fontAtlas)
		return;
	auto f = fontAtlas->fontMaps.find(size);
	const auto &fontMap = f->second;
	int max_chars = (int)text.length();

	if (max_chars <= 0)
		return;
	if (max_chars > 8192)
		return;
	if (!fontAtlas)
		return;
	if (!fontAtlas->fontTexture)
		return;
	auto *fontTexture = fontAtlas->fontTexture->GetSimulTexture();
	if (!fontTexture)
		return;
	vec4 transp = {0.f, 0.f, 0.f, 0.5f};
	// Calc width and draw background:
	float W = 0;
	float maxw = 0;
	int lines = 1;
	for (int i = 0; i < max_chars; i++)
	{
		char c = text[i];
		if (text[i] == 0)
			break;
		if (text[i] == '\n')
		{
			W = 0;
			lines++;
			continue;
		}
		int idx = (int)text[i] - 32;
		if (idx < 0 || idx > 100)
			continue;
		const teleport::core::Glyph &glyph = fontMap.glyphs[idx];
		W += glyph.xAdvance + 1.0f;
		maxw = std::max(W, maxw);
	}
	float ht = float(lineHeight);

	uint n = 0;

	platform::crossplatform::MultiviewGraphicsDeviceContext *mgdc = nullptr;
	size_t viewCount = 1;
	int passIndex = 0;
	EffectPass *pass = link ? link_singleViewPass : singleViewPass;
	EffectPass *bkg_pass = backgroundPass;
	
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		mgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
		if (mgdc)
		{
			pass = link?link_multiViewPass:multiViewPass;
			bkg_pass = backgroundPassMV;
			viewCount = mgdc->viewStructs.size();
			bool supportShaderViewID = renderPlatform->GetType() == crossplatform::RenderPlatformType::D3D11 ? false : true;
			passIndex = supportShaderViewID ? 0 : 1;
			SIMUL_ASSERT_WARN(supportShaderViewID, "Graphics API doesn't support SV_ViewID/gl_ViewIndex in the shader. Falling back to single view rendering.");
		}
	}
	if(!pass)
		return;
	float w = float(fontTexture->width);
	float h = float(fontTexture->length);
	// what size is a pixel, in metres?
	float pixelSizeMetres = lineHeight / float(size);
	if (max_chars > fontChars.count)
		fontChars.RestoreDeviceObjects(renderPlatform, max_chars, false, false, nullptr, "fontChars");
	FontChar *charList = fontChars.GetBuffer(deviceContext);
	if (!charList)
		return;
	// text_rect will be expressed in (-1,1) within the bounds of the canvas.
	for (size_t i = 0; i < viewCount; i++)
	{
		// line height in scale (0,1) relative to canvas height.
		float pixelHeight = (pixelSizeMetres / fabs(canvas.w));
		float pixelWidth = pixelSizeMetres / fabs(canvas.z);
		float lineHeight = fontMap.lineHeight * pixelHeight;
		float ytexel = 1.0f;
		// start at 0,1. Don't set x size yet.
		vec4 text_rect = vec4(0, 0, 0, lineHeight);
		float _y = lineHeight;
		float _x = 0.0f;
		for (int i = 0; i < fontChars.count; i++)
		{
			char c = text[i];
			if (c == 0)
				break;
			if (c == '\n')
			{
				_x = 0;
				_y += lineHeight;
				continue;
			}
			int idx = (int)c - 32;
			if (idx < 0 || idx >= fontMap.glyphs.size())
				continue;
			const teleport::core::Glyph &g = fontMap.glyphs[idx];
			if (idx > 0)
			{
				if (charList != nullptr)
				{
					FontChar &fontChar = charList[n];
					fontChar.text_rect = text_rect;
					fontChar.text_rect.x = _x + g.xOffset * pixelWidth;
					fontChar.text_rect.z = (float)(g.xOffset2 - g.xOffset) * pixelWidth;
					fontChar.text_rect.y = _y + g.yOffset * pixelHeight;
					fontChar.text_rect.w = (g.yOffset2 - g.yOffset) * pixelHeight;
					// xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap

					fontChar.texc = vec4(g.x0 / w, g.y0 / h, (g.x1 - g.x0) / w, (g.y1 - g.y0) / h);
				}
				n++;
			}
			_x += (g.xAdvance + 1) * pixelWidth;
		}
		float startpoint=0.5f-_x/2.f;
		if (centre)
		for (int i = 0; i < fontChars.count; i++)
		{
				FontChar &fontChar = charList[n];
				fontChar.text_rect.x += startpoint;
		}
	}
	n /= static_cast<uint>(viewCount);
	textConstants.colour = colour;
	textConstants.background = bkg;
	textConstants.background_rect[0] = canvas;
	textConstants.numChars = n;
	if (n > 0)
	{
		renderPlatform->SetConstantBuffer(deviceContext, &textConstants);
		renderPlatform->SetVertexBuffers(deviceContext, 0, 0, nullptr, nullptr);
		renderPlatform->SetTexture(deviceContext, textureResource, fontTexture);
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLELIST);
		if(link)
		{
			renderPlatform->ApplyPass(deviceContext, bkg_pass);
			renderPlatform->Draw(deviceContext, 6, 0);
			renderPlatform->UnapplyPass(deviceContext);
		}
		renderPlatform->ApplyPass(deviceContext, pass);
		fontChars.Apply(deviceContext, _fontChars);
#if !defined(__ANDROID__)
		renderPlatform->Draw(deviceContext, 6 * n, 0);
#endif
		effect->UnbindTextures(deviceContext);
		renderPlatform->UnapplyPass(deviceContext);
	}
}