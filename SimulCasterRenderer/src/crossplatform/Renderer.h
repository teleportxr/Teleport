// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/Pipeline.h"
#include "api/FrameBuffer.h"
#include "Camera.h"
#include "Light.h"
#include "Object.h"

namespace scr
{
	class Renderer
	{
	private:
		FrameBuffer* m_FrameBuffer;
		Pipeline* m_Pipeline;
		Camera* m_Camera;
		std::vector<Light*> m_Lights;
		std::deque<Object*> m_Objects;

	public:
		Renderer();

		void SubmitTargetFrameBuffer(FrameBuffer& framebuffer);
		void SubmitPipeline(Pipeline& pipeline);
		void SubmitCamera(Camera& camera);
		void SubmitLight(Light& light);
		void SubmitObject(Object& object);

		void Execute();

	private:
		void SortObjectsByDistance();
	};
}