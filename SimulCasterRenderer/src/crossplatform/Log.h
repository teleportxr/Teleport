#pragma once

#if defined(PLATFORM_ANDROID)
extern void RedirectStdCoutCerr();
#endif
enum class ClientLogPriority
{
	UNKNOWN = 0,
	DEFAULT,
	VERBOSE,
	DEBUG,
	INFO,
	WARNING,
	LOG_ERROR,
	FATAL,
	SILENT
};

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

extern void ClientLog(const char* fileTag, int lineno, ClientLogPriority prio, const char* fmt, ...);
#define LOG( ... ) 	ClientLog( __FILENAME__, __LINE__,ClientLogPriority::INFO, __VA_ARGS__ )
#define WARN( ... ) ClientLog( __FILENAME__, __LINE__, ClientLogPriority::WARNING,__VA_ARGS__ )
#define FAIL( ... ) {ClientLog( __FILENAME__, __LINE__, ClientLogPriority::LOG_ERROR,__VA_ARGS__ );exit(0);}