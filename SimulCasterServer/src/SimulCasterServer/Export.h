#pragma once

#define TELEPORTVR_EXPORT extern "C" __declspec(dllexport)

TELEPORTVR_EXPORT bool Initialize();
TELEPORTVR_EXPORT bool Uninitialize();