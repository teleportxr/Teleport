#pragma once
#include "CommonNetworking.h"
#include <fmt/core.h>
#include <string>

namespace teleport
{
	namespace core
	{
		//! Convert BackgroundMode enum to string
		inline const char* BackgroundModeToString(BackgroundMode mode)
		{
			switch (mode)
			{
			case BackgroundMode::NONE:
				return "NONE";
			case BackgroundMode::COLOUR:
				return "COLOUR";
			case BackgroundMode::TEXTURE:
				return "TEXTURE";
			case BackgroundMode::VIDEO:
				return "VIDEO";
			default:
				return "UNKNOWN";
			}
		}

		//! Convert LightingMode enum to string
		inline const char* LightingModeToString(LightingMode mode)
		{
			switch (mode)
			{
			case LightingMode::NONE:
				return "NONE";
			case LightingMode::TEXTURE:
				return "TEXTURE";
			case LightingMode::VIDEO:
				return "VIDEO";
			default:
				return "UNKNOWN";
			}
		}

		//! Format ClientDynamicLighting as JSON-style string for debugging, in declaration order.
		inline std::string ClientDynamicLightingToString(const ClientDynamicLighting& c)
		{
			return fmt::format(
				R"({{
    "specularPos": {{"x": {}, "y": {}}},
    "specularCubemapSize": {},
    "specularMips": {},
    "diffusePos": {{"x": {}, "y": {}}},
    "diffuseCubemapSize": {},
    "lightPos": {{"x": {}, "y": {}}},
    "lightCubemapSize": {},
    "specular_cubemap_texture_uid": {},
    "diffuse_cubemap_texture_uid": {},
    "lightingMode": "{}"
  }})",
				c.specularPos.x,  c.specularPos.y,
				c.specularCubemapSize,
				c.specularMips,
				c.diffusePos.x,   c.diffusePos.y,
				c.diffuseCubemapSize,
				c.lightPos.x,     c.lightPos.y,
				c.lightCubemapSize,
				c.specular_cubemap_texture_uid,
				c.diffuse_cubemap_texture_uid,
				LightingModeToString(c.lightingMode)
			);
		}

		//! Format SetLightingCommand as JSON-style string for debugging, in declaration order.
		inline std::string SetLightingCommandToString(const SetLightingCommand& cmd)
		{
			return fmt::format(
				R"({{
  "type": "SetLightingCommand",
  "ack_id": {},
  "clientDynamicLighting": {}
}})",
				cmd.ack_id,
				ClientDynamicLightingToString(cmd.clientDynamicLighting)
			);
		}

		//! Format SetupCommand as JSON-style string for debugging
		inline std::string SetupCommandToString(const SetupCommand& cmd)
		{
			return fmt::format(
				R"({{
  "type": "SetupCommand",
  "debug_stream": {},
  "debug_network_packets": {},
  "requiredLatencyMs": {},
  "idle_connection_timeout": {},
  "session_id": {},
  "draw_distance": {},
  "axesStandard": {},
  "audio_input_enabled": {},
  "using_ssl": {},
  "startTimestamp_utc_unix_us": {},
  "backgroundMode": "{}",
  "backgroundColour": {{"r": {}, "g": {}, "b": {}, "a": {}}},
  "backgroundTexture": {},
  "video_config": {{
    "video_width": {},
    "video_height": {},
    "depth_width": {},
    "depth_height": {}
  }},
  "note": "ClientDynamicLighting is sent via SetLightingCommand"
}})",
				cmd.debug_stream,
				cmd.debug_network_packets,
				cmd.requiredLatencyMs,
				cmd.idle_connection_timeout,
				cmd.session_id,
				cmd.draw_distance,
				static_cast<int>(cmd.axesStandard),
				static_cast<int>(cmd.audio_input_enabled),
				cmd.using_ssl,
				cmd.startTimestamp_utc_unix_us,
				BackgroundModeToString(cmd.backgroundMode),
				cmd.backgroundColour.x,
				cmd.backgroundColour.y,
				cmd.backgroundColour.z,
				cmd.backgroundColour.w,
				cmd.backgroundTexture,
				cmd.video_config.video_width,
				cmd.video_config.video_height,
				cmd.video_config.depth_width,
				cmd.video_config.depth_height
			);
		}
	}
}
