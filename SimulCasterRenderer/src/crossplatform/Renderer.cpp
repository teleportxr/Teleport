// (C) Copyright 2018-2019 Simul Software Ltd

#include "Renderer.h"

using namespace scr;

Renderer::Renderer()
{
}

void Renderer::SubmitTargetFrameBuffer(FrameBuffer& framebuffer)
{
	m_FrameBuffer = &framebuffer;
}

void Renderer::SubmitPipeline(Pipeline& pipeline)
{
	m_Pipeline = &pipeline;
}

void Renderer::SubmitCamera(Camera& camera)
{
	m_Camera = &camera;
}

void Renderer::SubmitLight(Light& light)
{
	m_Lights.push_back(&light);
}

void Renderer::SubmitObject(Object& object)
{
	m_Objects.push_back(&object);
}

void Renderer::Execute()
{
	m_FrameBuffer->Bind();
	m_FrameBuffer->Clear(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	m_Pipeline->Bind();

	m_Camera->UpdateView();
	
	for (auto& light : m_Lights)
		light->UpdateLightUBO();

	SortObjectsByDistance();
	while (!m_Objects.empty())
	{
		Object* obj = m_Objects.front();

		obj->Bind();
		m_Pipeline->Draw(Pipeline::TopologyType::TRIANGLE_LIST, obj->GetIndexBufferCount());

		m_Objects.pop_front();
	}
}

//private
void Renderer::SortObjectsByDistance()
{
	const vec3& camPos = m_Camera->GetPosition();
	std::sort(m_Objects.begin(), m_Objects.end(),
		[&](Object* obj_a, Object* obj_b)
		{
			float length_a = (obj_a->GetTranslation() - camPos).Length();
			float length_b = (obj_b->GetTranslation() - camPos).Length();
			return length_a < length_b;
		});
}
