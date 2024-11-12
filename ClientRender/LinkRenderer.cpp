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
	linkEffect.reset();
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

void LinkRenderer::RenderLink(platform::crossplatform::GraphicsDeviceContext &deviceContext, const LinkRender &linkRender,bool highlight)
{
	if (!linkEffect || reload_shaders)
	{
		linkEffect=renderPlatform->GetOrCreateEffect("link");
		link_tech=linkEffect->GetTechniqueByName("link");
		reload_shaders=false;
	}
	if(!linkEffect||!link_tech)
		return;
	linkConstants.radius=1.0f;
	vec4 highlight_colour={1.8f, 1.8f,0.8f, 1.f};
	vec4 colour = {0.8f, 0.8f, 0.9f, 1.f};
	linkConstants.linkColour = highlight?highlight_colour:colour;
	linkConstants.time = linkRender.time;
	static float dtS=0.001f;
	linkRender.time+=(float) dtS *(highlight ? 5.f : 1.f);
	linkConstants.distanceToDepthParam = deviceContext.viewStruct.GetDepthToLinearDistanceParameters(1.0f).x;
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &linkConstants);
	auto *pass=link_tech->GetPass("singleview");
	renderPlatform->ApplyPass(deviceContext, pass);
	renderPlatform->SetTopology(deviceContext, platform::crossplatform::Topology::TRIANGLESTRIP);
	renderPlatform->DrawQuad(deviceContext);
	renderPlatform->UnapplyPass(deviceContext);
}