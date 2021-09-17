// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <libavstream/platforms/platform_windows.hpp>

#include <string>
#include <memory>

namespace { // internal linkage

std::string wideCharToUnicode(const std::wstring& wstr)
{
	const int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	const std::unique_ptr<char[]> buffer(new char[bufferSize]);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.get(), bufferSize, nullptr, nullptr);
	return std::string(buffer.get());
}

std::wstring unicodeToWideChar(const std::string& str)
{
	const int bufferSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	const std::unique_ptr<wchar_t[]> buffer(new wchar_t[bufferSize]);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer.get(), bufferSize);
	return std::wstring(buffer.get());
}

} // internal linkage

namespace avs {
	LARGE_INTEGER PlatformWindows::frequency = { 0,0 };
LibraryHandle PlatformWindows::openLibrary(const char* filename)
{
	return LoadLibrary(unicodeToWideChar(filename).c_str());
}
	
bool PlatformWindows::closeLibrary(LibraryHandle hLibrary)
{
	return FreeLibrary(hLibrary) == TRUE;
}
	
ProcAddress PlatformWindows::getProcAddress(LibraryHandle hLibrary, const char* function)
{
	return GetProcAddress(hLibrary, function);
}
	
Timestamp PlatformWindows::getTimestamp()
{
	int64_t result;
	QueryPerformanceCounter((LARGE_INTEGER*)&result);
	return *((LARGE_INTEGER*)&result);
}

double PlatformWindows::getTimeElapsed(const Timestamp& tBegin, const Timestamp& tEnd)
{
	if(!frequency.QuadPart)
		QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER elapsedCounter;
	elapsedCounter.QuadPart = tEnd.QuadPart - tBegin.QuadPart;
	// Convert to milliseconds
	return (elapsedCounter.QuadPart * 1000.0) / (long double)frequency.QuadPart;
}

double PlatformWindows::getTimeElapsedInSeconds(const Timestamp& tBegin, const Timestamp& tEnd)
{
	if (!frequency.QuadPart)
		QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER elapsedCounter;
	elapsedCounter.QuadPart = tEnd.QuadPart - tBegin.QuadPart;

	return (elapsedCounter.QuadPart) / (double)frequency.QuadPart;
}

SystemTime PlatformWindows::getSystemTime()
{
	SYSTEMTIME systime;
	GetSystemTime(&systime);
	return SystemTime{ systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds };
}

} // avs
