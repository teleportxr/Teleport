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

extern void ClientLog(const char* fileTag, int lineno, ClientLogPriority prio, const char* fmt, ...);
#define LOG( ... ) 	ClientLog( __FILE__, __LINE__,ClientLogPriority::INFO, __VA_ARGS__ )
#define WARN( ... ) ClientLog( __FILE__, __LINE__, ClientLogPriority::WARNING,__VA_ARGS__ )
#define FAIL( ... ) {ClientLog( __FILE__, __LINE__, ClientLogPriority::LOG_ERROR,__VA_ARGS__ );exit(0);}