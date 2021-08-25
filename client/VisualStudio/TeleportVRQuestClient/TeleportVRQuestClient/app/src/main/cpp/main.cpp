#include <dlfcn.h>
#include <jni.h>
#include <errno.h>
#include <sys/resource.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android/window.h>
#include <android_native_app_glue.h>

#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }

EXTERN_C_BEGIN;

//Forward Declarations
ANativeWindow* gp_AndroidWindow = nullptr;
bool g_CloseWindow = false;
static void handle_cmd(android_app* app, int32_t cmd)
{
	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		gp_AndroidWindow = app->window;
		g_CloseWindow = false;
		break;
	case APP_CMD_TERM_WINDOW:
		g_CloseWindow = true;
		break;
	default:
		__android_log_write(ANDROID_LOG_INFO, "TeleportVRQuestClient", "Event not handled");
	}
}

void android_main(struct android_app* app) 
{
	app->onAppCmd = handle_cmd;
	int events;
	android_poll_source* source;
	while (gp_AndroidWindow == nullptr) {
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);
		}
	}

	while (!g_CloseWindow)
	{
		if (ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);
		}
	}
}

EXTERN_C_END;