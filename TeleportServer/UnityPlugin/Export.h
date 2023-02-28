#pragma once
#if PLATFORM_WINDOWS
#define TELEPORT_EXPORT extern "C" __declspec(dllexport)
#else
#define TELEPORT_EXPORT extern "C" __attribute__((visibility("default")))
#endif