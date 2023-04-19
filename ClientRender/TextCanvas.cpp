#include "TextCanvas.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Macros.h"

using namespace clientrender;
using namespace platform;
using namespace crossplatform;

size_t TextCanvas::count=0;
bool TextCanvas::recompile=false;
Effect							*TextCanvas::effect=nullptr;
EffectTechnique					*TextCanvas::tech=nullptr;
EffectPass					*TextCanvas::singleViewPass=nullptr;
EffectPass					*TextCanvas::multiViewPass=nullptr;
ShaderResource					TextCanvas::textureResource;
ShaderResource					TextCanvas::_fontChars;
crossplatform::RenderPlatform*					TextCanvas::renderPlatform;
ConstantBuffer<TextConstants>	TextCanvas::textConstants;


TextCanvas::TextCanvas(const TextCanvasCreateInfo &t)
	:IncompleteTextCanvas(t.uid)
	,textCanvasCreateInfo(t)
{
	count++;
}

TextCanvas::~TextCanvas()
{
	InvalidateDeviceObjects();

}
void TextCanvas::RestoreCommonDeviceObjects(platform::crossplatform::RenderPlatform *r)
{
	if(renderPlatform==r)
		return;
	renderPlatform=r;
	textConstants.InvalidateDeviceObjects();
	textConstants.RestoreDeviceObjects(renderPlatform);
	
	Recompile();
	
}

void TextCanvas::InvalidateCommonDeviceObjects()
{
	if(!renderPlatform)
		return;
	renderPlatform=nullptr;
	SAFE_DELETE(effect);
	//SAFE_DELETE(singleViewPass);
//	SAFE_DELETE(multiViewPass);
				
	textConstants.InvalidateDeviceObjects();
}

void TextCanvas::RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r)
{
	if(renderPlatform==r)
		return;
	RestoreCommonDeviceObjects(r);
}

void TextCanvas::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	fontChars.InvalidateDeviceObjects();
	count--;
	if(!count)
		InvalidateCommonDeviceObjects();
}

void TextCanvas::SetFontAtlas(std::shared_ptr<clientrender::FontAtlas> f)
{
	fontAtlas=f;
}

void TextCanvas::Recompile()
{
	recompile = false;
	SAFE_DELETE(effect);
	effect			=renderPlatform->CreateEffect("canvas_text");
	textConstants.LinkToEffect(effect,"TextConstants");
	tech			=effect->GetTechniqueByName("text");
	
	singleViewPass			=tech->GetPass("singleview");
	multiViewPass			=tech->GetPass("multiview");

	textureResource	=effect->GetShaderResource("fontTexture");
	_fontChars		=effect->GetShaderResource("fontChars");
}

void TextCanvas::Render(GraphicsDeviceContext &deviceContext,platform::crossplatform::ConstantBuffer<CameraConstants> &cameraConstants
	,platform::crossplatform::ConstantBuffer<StereoCameraConstants> &stereoCameraConstants
	,platform::crossplatform::Texture *fontTexture)
{
	if(!renderPlatform)
		RestoreDeviceObjects(deviceContext.renderPlatform);
	if(!fontAtlas)
		return;
	auto f= fontAtlas->fontMaps.find(textCanvasCreateInfo.size);
	const auto &fontMap=f->second;

	int max_chars = (int)textCanvasCreateInfo.text.length();
	
	if(max_chars<=0)
		return;
	if(max_chars>8192)
		return;
	if (recompile)
		Recompile();
	vec4 transp = { 0.f,0.f,0.f,0.5f };
	vec4 white = { 1.f,1.f,1.f,1.f };
	textConstants.colour = textCanvasCreateInfo.colour;
	textConstants.background_rect[0] = {-textCanvasCreateInfo.width/2.0f,textCanvasCreateInfo.height/2.0f,textCanvasCreateInfo.width,-textCanvasCreateInfo.height};
	// Calc width and draw background:
	float W = 0;
	float maxw = 0;
	int lines = 1;
	for (int i = 0; i < max_chars; i++)
	{
		char c=textCanvasCreateInfo.text[i];
		if (textCanvasCreateInfo.text[i] == 0)
			break;
		if (textCanvasCreateInfo.text[i] == '\n')
		{
			W = 0;
			lines++;
			continue;
		}
		int idx = (int)textCanvasCreateInfo.text[i] - 32;
		if (idx < 0 || idx>100)
			continue;
		const teleport::core::Glyph& glyph = fontMap.glyphs[idx];
		W += glyph.xAdvance + 1.0f;
		maxw = std::max(W, maxw);
	}
	float ht = float(textCanvasCreateInfo.lineHeight);

	uint n = 0;

	crossplatform::MultiviewGraphicsDeviceContext* mgdc=nullptr;
	size_t viewCount=1;
	int passIndex=0;
	EffectPass *pass=singleViewPass;
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		mgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
		if(mgdc)
		{
			pass=multiViewPass;
			viewCount = mgdc->viewStructs.size();
			bool supportShaderViewID = renderPlatform->GetType() == crossplatform::RenderPlatformType::D3D11 ? false : true;
			passIndex = supportShaderViewID ? 0 : 1;
			SIMUL_ASSERT_WARN(supportShaderViewID, "Graphics API doesn't support SV_ViewID/gl_ViewIndex in the shader. Falling back to single view rendering.");
		}
	}
	float w=float(fontTexture->width);
	float h=float(fontTexture->length);
	// what size is a pixel, in metres?
	float pixelSizeMetres=textCanvasCreateInfo.lineHeight/float(textCanvasCreateInfo.size);
	if (max_chars > fontChars.count)
		fontChars.RestoreDeviceObjects(renderPlatform, max_chars, false, false, nullptr, "fontChars");
	FontChar* charList = fontChars.GetBuffer(deviceContext);
	if(!charList)
		return;
	// text_rect will be expressed in (-1,1) within the bounds of the canvas.
	for (size_t i = 0; i < viewCount; i++)
	{
		// line height in scale (0,1) relative to canvas height.
		float pixelHeight=(pixelSizeMetres/textCanvasCreateInfo.height);
		float pixelWidth=(pixelSizeMetres/textCanvasCreateInfo.width);
		float lineHeight = fontMap.lineHeight*pixelHeight;
		float ytexel = 1.0f;
		// start at 0,1. Don't set x size yet.
		vec4 text_rect = vec4(0, 0, 0, lineHeight);
		float _y=lineHeight;
		float _x = 0.0f;
		for (int i = 0; i < fontChars.count; i++)
		{
			char c=textCanvasCreateInfo.text[i];
			if (c == 0)
				break;
			if (c== '\n')
			{
				_x = 0;
				_y += lineHeight;
				continue;
			}
			int idx = (int)c - 32;
			if (idx < 0 || idx>=fontMap.glyphs.size())
				continue;
			const teleport::core::Glyph& g = fontMap.glyphs[idx];
			if (idx > 0)
			{
				if (charList != nullptr)
				{
					FontChar& fontChar = charList[n];
					fontChar.text_rect = text_rect;
					fontChar.text_rect.x =  _x+g.xOffset*pixelWidth;
					fontChar.text_rect.z =  (float) (g.xOffset2-g.xOffset) * pixelWidth;
					fontChar.text_rect.y =  _y+g.yOffset* pixelHeight;
					fontChar.text_rect.w =  (g.yOffset2-g.yOffset) * pixelHeight;
					//xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap

					fontChar.texc = vec4(g.x0/w, g.y0/h, (g.x1 - g.x0)/w, (g.y1-g.y0)/h);
				}
				n++;
			}
			_x += (g.xAdvance  + 1)*pixelWidth;
		}
	}
	n /= static_cast<uint>(viewCount);
	textConstants.numChars = n;
	if (n > 0)
	{
		effect->SetTexture(deviceContext, textureResource, fontTexture);
		renderPlatform->ApplyPass(deviceContext,pass);
		effect->SetConstantBuffer(deviceContext, &textConstants);
		effect->SetConstantBuffer(deviceContext, &cameraConstants);
		if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
			effect->SetConstantBuffer(deviceContext, &stereoCameraConstants);
		renderPlatform->SetVertexBuffers(deviceContext, 0, 0, nullptr, nullptr);
		fontChars.Apply(deviceContext, effect, _fontChars);
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLELIST);
#if !defined(__ANDROID__)
		renderPlatform->Draw(deviceContext, 6 * n, 0);
#endif
		effect->UnbindTextures(deviceContext);
		renderPlatform->UnapplyPass(deviceContext);
	}
}