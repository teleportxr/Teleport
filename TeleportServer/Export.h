#pragma once
#ifdef DOXYGEN
#define TELEPORT_EXPORT
#else
#if PLATFORM_WINDOWS
#define TELEPORT_EXPORT extern "C" __declspec(dllexport)
#else
#define TELEPORT_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#endif

#if defined(_MSC_VER)
		#pragma message("DEC")
    //  Microsoft
    #define TELEPORT_EXPORT_DEC __declspec(dllexport)
    #define TELEPORT_IMPORT_DEC __declspec(dllimport)
#elif defined(__GNUC__)
    //  GCC or Clang
#define TELEPORT_EXPORT_DEC __attribute__((visibility("default")))
#define TELEPORT_IMPORT_DEC
#else
    //  do nothing and hope for the best?
#define TELEPORT_EXPORT_DEC
#define TELEPORT_IMPORT_DEC
    #pragma warning Unknown dynamic link import/export semantics.
#endif

		#pragma message("1")
#if defined(TELEPORT_EXPORT_SERVER_DLL) && !defined(DOXYGEN)
		#pragma message("2")
	// In this lib:
	// If we're building dll libraries but not in this library IMPORT the classes
	#if !defined(TELEPORT_SERVER_DLL) 
		#pragma message("3")
		#define TELEPORT_SERVER_API TELEPORT_IMPORT_DEC
		#pragma message("Importing TELEPORT_SERVER_API")
	#else
		#pragma message("Exporting TELEPORT_SERVER_API")
	// In ALL OTHER CASES we EXPORT the classes!
#define TELEPORT_SERVER_API TELEPORT_EXPORT_DEC
	#endif
#else
#define TELEPORT_SERVER_API
#endif

#pragma warning(disable : 4251)
#pragma warning(disable :  4275)