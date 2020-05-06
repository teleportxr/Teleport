#pragma once


extern void RedirectStdCoutCerr();
extern void ClientLog(const char* fileTag, int lineno, const char* msg_type, const char* fmt, ...);

#define LOG( ... ) 	ClientLog( __FILE__, __LINE__,"info", __VA_ARGS__ )
#define WARN( ... ) ClientLog( __FILE__, __LINE__, "warning",__VA_ARGS__ )
#define FAIL( ... ) {ClientLog( __FILE__, __LINE__, "error",__VA_ARGS__ );exit(0);}