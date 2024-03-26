// (C) Copyright 2018-2024 Simul Software Ltd
#include "ClientRender/LinkRenderer.h"
#include "Platform/CrossPlatform/Macros.h"

using namespace teleport;
using namespace clientrender;

LinkRenderer::LinkRenderer()
{
}

LinkRenderer::~LinkRenderer()
{
	InvalidateDeviceObjects();
}

void LinkRenderer::RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r)
{
	reload_shaders = true;
	renderPlatform = r;
	linkConstants.RestoreDeviceObjects(r);
}

void LinkRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE(linkEffect);
	reload_shaders = true;
	linkConstants.InvalidateDeviceObjects();
	renderPlatform=nullptr;
}

void LinkRenderer::RecompileShaders()
{
	if(renderPlatform)
		renderPlatform->ScheduleRecompileEffects({"link"}, [this]()
											 { reload_shaders = true; });
}

void LinkRenderer::RenderLink(platform::crossplatform::GraphicsDeviceContext &deviceContext, const LinkRender &linkRender)
{
	if (!linkEffect || reload_shaders)
	{
		linkEffect=renderPlatform->CreateEffect("link");
		reload_shaders=false;
	}
	if(!linkEffect)
		return;
	linkConstants.radius=1.0f;
	linkConstants.time = (float)deviceContext.predictedDisplayTimeS;
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &linkConstants);
	auto *pass=linkEffect->GetTechniqueByName("link")->GetPass("singleview");
	renderPlatform->ApplyPass(deviceContext, pass);
	renderPlatform->SetTopology(deviceContext, platform::crossplatform::Topology::TRIANGLESTRIP);
	renderPlatform->DrawQuad(deviceContext);
	renderPlatform->UnapplyPass(deviceContext);
	//
	vec4 colour = {1.f, 1.f, 0.f, 1.f};
	vec4 background = {0, 0, 0, 1.f};
	//renderPlatform->PrintAt3dPos(deviceContext, l.position, l.url.c_str(), colour, background);
}