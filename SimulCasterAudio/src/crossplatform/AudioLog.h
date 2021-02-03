#pragma once

extern void AudioLog(const char* fileTag, int lineno, const char* msg_type, const char* fmt, ...);

#define LOG( ... ) 	AudioLog( __FILE__, __LINE__,"info", __VA_ARGS__ )
#define WARN( ... ) AudioLog( __FILE__, __LINE__, "warning",__VA_ARGS__ )
#define FAIL( ... ) {AudioLog( __FILE__, __LINE__, "error",__VA_ARGS__ );exit(0);}