// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#if defined(_MSC_VER)
#	define __DECL_EXPORT __declspec(dllexport)
#	define __DECL_IMPORT __declspec(dllimport)
#else
#	define __DECL_EXPORT
#	define __DECL_IMPORT
#endif

#if defined(LIBAVSTREAM_SHARED) && !defined(DOXYGEN)
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


// Migrating away from the above confusing mess to a cleaner C++ API:
#define AVSTREAM_COMPLETEINTERFACE_API(Type) \
	public: \
	inline Type& d() { return *this; } \
	inline Type* d_ptr() { return this; } \
	inline const Type& d() const { return *m_d; } \
	inline const Type* d_ptr() const { return m_d; }

#define AVSTREAM_COMPLETEINTERFACE_FINAL(Type) \
	AVSTREAM_COMPLETEINTERFACE_API(Type) \
	private: \
	Type *m_d;