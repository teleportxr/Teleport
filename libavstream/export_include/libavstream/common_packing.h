#pragma once
#ifndef AVS_PACKED
	#if defined(__GNUC__) || defined(__clang__)
		#define AVS_PACKED __attribute__ ((packed,aligned(1)))
	#else
		#define AVS_PACKED
	#endif
#endif

#define DEBUG_CHECK_STRUCTSIZE(StructName)\
	char *tmp##StructName=(char[sizeof(StructName)])#StructName;