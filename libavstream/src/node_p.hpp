// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <common_p.hpp>
#include <libavstream/node.hpp>

#include <vector>

namespace avs
{

struct Link
{
	Node* targetNode = nullptr;
	int   targetSlot = 0;

	operator bool() const { return targetNode != nullptr; }
};

struct Node::Private
{
	AVSTREAM_PRIVATEINTERFACE_BASE(Node)
	virtual ~Private() = default;

	std::vector<Link> m_inputs;
	std::vector<Link> m_outputs;
};

} // avs