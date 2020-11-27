// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

#include "Node.h"

#include "api/FrameBuffer.h"
#include "api/UniformBuffer.h"

namespace scr
{
	class Light
	{
	public:
		// Matches Unity... also scr::LightType
		enum class Type : uint32_t
		{
			SPOT=0,
			DIRECTIONAL=1,
			POINT=2,
			AREA=3,
			RECTANGLE=3,
			DISC=4
		};
		struct LightCreateInfo
		{
			const RenderPlatform* renderPlatform;
			Type type;
			avs::vec3 position;
			avs::vec4 lightColour;
			float lightRadius;
			avs::vec3 direction;
			quat orientation;
			std::shared_ptr<Texture> shadowMapTexture;
			avs::uid uid;
		};

		struct LightData //Layout conformant to GLSL std140
		{
			mat4 lightSpaceTransform;
			avs::vec4 colour;
			avs::vec3 position;
			float power;		 //Strength or Power of the light in Watts equilavent to Radiant Flux in Radiometry.
			avs::vec3 direction;
			float is_point;
			float is_spot;
			float radius;		// "point" light is a sphere.
			unsigned uid32;		// lowest 32 bits of the uid.
			float pad1;
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
		
		const LightCreateInfo &GetLightCreateInfo()
		{
			return m_CI;
		}
		inline const ShaderResource& GetDescriptorSet() const { return m_ShaderResource; }
		inline std::shared_ptr<Texture>& GetShadowMapTexture() { return m_CI.shadowMapTexture; }
		const LightData* GetLightData() const;
		static const std::vector<LightData> &GetAllLightData() 
		{
			return s_LightData;
		}
		inline bool IsValid() { return m_IsValid; }

	private:
		void UpdateLightSpaceTransform();
	};
}