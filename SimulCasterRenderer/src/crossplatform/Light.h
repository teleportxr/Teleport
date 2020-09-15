// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

#include "Actor.h"

#include "api/FrameBuffer.h"
#include "api/UniformBuffer.h"

namespace scr
{
	class Light
	{
	public:
		enum class Type : uint32_t
		{
			POINT=0,
			DIRECTIONAL=1,
			SPOT=2,
			AREA=3
		};
		struct LightCreateInfo
		{
			const RenderPlatform* renderPlatform;
			Type type;
			avs::vec3 position;
			avs::vec4 lightColour;
			avs::vec3 direction;
			quat orientation;
			std::shared_ptr<Texture> shadowMapTexture;
		};

		struct LightData //Layout conformant to GLSL std140
		{
			avs::vec4 colour;
			avs::vec3 position;
			float power;		 //Strength or Power of the light in Watts equilavent to Radiant Flux in Radiometry.
			avs::vec3 direction;
			float _pad;
			mat4 lightSpaceTransform;
		};
	
	private:
		LightCreateInfo m_CI;
		static bool s_UninitialisedUB;
		static std::shared_ptr<UniformBuffer> s_UB;

		static const uint32_t s_MaxLights;
		static std::vector<LightData> s_LightData;
		size_t m_LightID;
		bool m_IsValid = false;

		std::shared_ptr<Sampler> m_ShadowMapSampler;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;

	public:
		Light(LightCreateInfo* pLightCreateInfo);
		~Light() = default;

		void UpdatePosition(const avs::vec3& position);
		void UpdateOrientation(const quat& orientation);

		inline const ShaderResource& GetDescriptorSet() const { return m_ShaderResource; }
		inline std::shared_ptr<Texture>& GetShadowMapTexture() { return m_CI.shadowMapTexture; }
		const LightData& GetLightData() const;
		static const std::vector<LightData> &GetAllLightData() 
		{
			return s_LightData;
		}
		inline bool IsValid() { return m_IsValid; }

	private:
		void UpdateLightSpaceTransform();
	};
}