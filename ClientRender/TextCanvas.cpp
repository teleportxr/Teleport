#include "TextCanvas.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Macros.h"

using namespace teleport;
using namespace clientrender;
using namespace platform;
using namespace crossplatform;

TextCanvas::TextCanvas(const TextCanvasCreateInfo &t)
	:IncompleteTextCanvas(t.uid,"")
	,textCanvasCreateInfo(t)
{
}

TextCanvas::~TextCanvas()
{
	InvalidateDeviceObjects();

}

void TextCanvas::RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r)
{
}

void TextCanvas::InvalidateDeviceObjects()
{
	fontChars.InvalidateDeviceObjects();
}

void TextCanvas::SetFontAtlas(std::shared_ptr<clientrender::FontAtlas> f)
{
	fontAtlas=f;
}
