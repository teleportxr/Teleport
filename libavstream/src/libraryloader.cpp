// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include "libraryloader.hpp"

namespace avs {

	LibraryLoader::LibraryLoader(const std::string& name)
#if defined(PLATFORM_WINDOWS)
		: m_name(name + ".dll")
#else
		: m_name("lib" + name + ".so")
#endif
		, m_handle(nullptr)
		, m_refcount(0)
	{}

	LibraryLoader::~LibraryLoader()
	{
		if (m_handle)
		{
			m_refcount = 1;
			unload();
		}
	}

	LibraryHandle LibraryLoader::load()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_refcount == 0)
		{
			m_handle = Platform::openLibrary(m_name.c_str());
			if (!m_handle)
			{
				return nullptr;
			}
		}
		++m_refcount;
		return m_handle;
	}

	void LibraryLoader::unload()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_refcount > 0)
		{
			if (--m_refcount == 0)
			{
				Platform::closeLibrary(m_handle);
				m_handle = nullptr;
			}
		}
	}

} // avs