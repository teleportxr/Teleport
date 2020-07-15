// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/abi.hpp>

#define AVSTREAM_PRIVATEINTERFACE_API(Type) \
	friend Type; \
	inline Type& q() { return static_cast<Type&>(*m_q); } \
	inline Type* q_ptr() { return static_cast<Type*>(m_q); } \
	inline const Type& q() const { return static_cast<const Type&>(*m_q); } \
	inline const Type* q_ptr() const { return static_cast<const Type*>(m_q); }

#define AVSTREAM_PRIVATEINTERFACE_BASE(Type) \
	AVSTREAM_PRIVATEINTERFACE_API(Type) \
	explicit Private(Type* q_ptr) : m_q(q_ptr) {} \
	Type *m_q;

#define AVSTREAM_PRIVATEINTERFACE(Type, BaseClass) \
	AVSTREAM_PRIVATEINTERFACE_API(Type) \
	explicit Private(Type* q_ptr) : BaseClass::Private(q_ptr) {}
