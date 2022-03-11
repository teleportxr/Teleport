// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "ClientRender/GeometryCache.h"
#include "ClientRender/ResourceCreator.h"
#include <libavstream/src/platform.hpp>
#include "TeleportClient/SessionClient.h"

namespace clientrender
{
	enum
	{
		NO_OSD=0,
		CAMERA_OSD,
		NETWORK_OSD,
		GEOMETRY_OSD,
		TEXTURES_OSD,
		TAG_OSD,
		CONTROLLER_OSD,
		NUM_OSDS
	};
	//! Timestamp of when the system started.
	extern avs::Timestamp platformStartTimestamp;	
	//! Base class for a renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class Renderer
	{
	public:
		Renderer(clientrender::NodeManager *localNodeManager,clientrender::NodeManager *remoteNodeManager);

		void SetMinimumPriority(int32_t p)
		{
			minimumPriority=p;
		}
		int32_t GetMinimumPriority() const
		{
			return minimumPriority;
		}
		virtual void ConfigureVideo(const avs::VideoConfig &vc)=0;
		avs::SetupCommand lastSetupCommand;
		avs::SetupLightingCommand lastSetupLightingCommand;

		float framerate = 0.0f;
		void Update(double timestamp_ms);
	protected:
		int show_osd = NO_OSD;
		double previousTimestamp=0.0;
		int32_t minimumPriority=0;
		bool using_vr = true;
		clientrender::GeometryCache localGeometryCache;
		clientrender::ResourceCreator localResourceCreator;
	public:
		clientrender::GeometryCache geometryCache;
		clientrender::ResourceCreator resourceCreator;

	};
}