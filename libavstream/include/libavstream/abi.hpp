// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#if defined(_MSC_VER)
#	define __DECL_EXPORT __declspec(dllexport)
#	define __DECL_IMPORT __declspec(dllimport)
#else
#	define __DECL_EXPORT
#	define __DECL_IMPORT
#endif

#if defined(LIBAVSTREAM_SHARED)
#	if defined(LIBAVSTREAM_EXPORTS)
#		define AVSTREAM_API __DECL_EXPORT
#	else
#		define AVSTREAM_API __DECL_IMPORT
#	endif
#else
#	define AVSTREAM_API
#endif

#define AVSTREAM_PUBLICINTERFACE_API \
	public: \
	struct Private; \
	friend Private; \
	inline Private& d() { return reinterpret_cast<Private&>(*m_d); } \
	inline Private* d_ptr() { return reinterpret_cast<Private*>(m_d); } \
	inline const Private& d() const { return reinterpret_cast<const Private&>(*m_d); } \
	inline const Private* d_ptr() const { return reinterpret_cast<const Private*>(m_d); }

#define AVSTREAM_PUBLICINTERFACE_BASE(Type) \
	AVSTREAM_PUBLICINTERFACE_API \
	protected: \
	explicit Type(Type::Private* d_ptr); \
	Private *m_d;

#define AVSTREAM_PUBLICINTERFACE_FINAL(Type) \
	AVSTREAM_PUBLICINTERFACE_API \
	private: \
	Private *m_d;

#define AVSTREAM_PUBLICINTERFACE(Type) \
	AVSTREAM_PUBLICINTERFACE_API