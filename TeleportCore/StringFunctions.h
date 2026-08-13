#pragma once
#include <cstdlib>
#include <string>

#ifndef _MSC_VER
	#define strncpy_s(txt,src,len) strncpy(txt, src, len)
#endif

namespace teleport
{
	namespace core
	{
		// Here we implement special cases of std::wofstream and wifstream that are able to convert guid_to_uid to guids and vice versa.
		inline std::string WStringToString(const std::wstring& text)
		{
			size_t origsize = text.length() + 1;
			const size_t newsize = origsize;
			char* cstring = new char[newsize];

#ifdef _MSC_VER
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, cstring, (size_t)origsize, text.c_str(), (size_t)newsize);
#else
			wcstombs(cstring, text.c_str(), (size_t)newsize);
#endif
			std::string str;
			str = std::string(cstring);
			delete[] cstring;
			return str;
		}
		inline std::wstring StringToWString(const std::string& text)
		{
			size_t origsize = text.length() + 1;
			const size_t newsize = origsize;
			wchar_t* wcstring = new wchar_t[newsize + 2];
#ifdef _MSC_VER
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wcstring, origsize, text.c_str(), _TRUNCATE);
#else
			mbstowcs(wcstring, text.c_str(), origsize);
#endif
			std::wstring str(wcstring);
			delete[] wcstring;
			return str;
		}
		//! The value of an environment variable, or an empty string if it is not set.
		//! Uses _dupenv_s on Windows to avoid the CRT deprecation warning for getenv.
		//! (Not called GetEnvironmentVariable: windows.h defines that name away.)
		inline std::string GetEnvVar(const char *name)
		{
#ifdef _WIN32
			char *value = nullptr;
			size_t size = 0;
			if (_dupenv_s(&value, &size, name) != 0 || !value) return {};
			std::string result(value);
			free(value);
			return result;
#else
			const char *value = std::getenv(name);
			return value ? value : "";
#endif
		}
	}
}