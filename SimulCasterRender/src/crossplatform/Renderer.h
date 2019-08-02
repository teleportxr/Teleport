// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Light.h"
#include "Object.h"

namespace scr
{
	class Renderer
	{
	private:
		std::vector<const Light*> m_Lights;
		std::deque<const Object*> m_Objects;

	public:
		Renderer();

		void SubmitLight(const Light& light);
		void SubmitObject(const Object& object);

		void Execute();
	};
}