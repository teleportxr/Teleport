// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include <optional>
#include <vector>
#include "node_p.hpp"

using namespace avs;

PipelineNode::PipelineNode(PipelineNode::Private* d_ptr)
	: m_d(d_ptr)
{}

PipelineNode::~PipelineNode()
{
	unlinkAll();
	delete m_d;
}

Result PipelineNode::link(PipelineNode& source, int sourceSlot, PipelineNode& target, int targetSlot)
{
	assert(sourceSlot >= 0);
	assert(targetSlot >= 0);

	if (size_t(sourceSlot) >= source.d().m_outputs.size())
	{
		return Result::Node_InvalidSlot;
	}
	if (size_t(targetSlot) >= target.d().m_inputs.size())
	{
		return Result::Node_InvalidSlot;
	}

	source.unlinkOutput(sourceSlot);
	target.unlinkInput(targetSlot);

	if (Result result = target.onInputLink(targetSlot, &source))
	{
		target.d().m_inputs[targetSlot] = { &source, sourceSlot };
	}
	else {
		return result;
	}

	if (Result result = source.onOutputLink(sourceSlot, &target))
	{
		source.d().m_outputs[sourceSlot] = { &target, targetSlot };
	}
	else {
		target.onInputUnlink(targetSlot, &source);
		target.d().m_inputs[targetSlot] = {};
		return result;
	}
	return Result::OK;
}

Result PipelineNode::link(PipelineNode& source, PipelineNode& target)
{
	std::optional<int> sourceSlot;
	for (int i = 0; i < (int)source.getNumOutputSlots(); ++i)
	{
		if (!source.isOutputLinked(i))
		{
			sourceSlot = i;
			break;
		}
		else if(source.getOutput(i)==&target)
		{
			// already linked.
			return avs::Result::OK;
		}
	}
	if (!sourceSlot.has_value())
	{
		AVSLOG(Error) << "PipelineNode: Cannot link nodes: no free output slot found on source.\n";
		return Result::Node_LinkFailed;
	}

	std::optional<int> targetSlot;
	for (int i = 0; i < (int)target.getNumInputSlots(); ++i)
	{
		if (!target.isInputLinked(i))
		{
			targetSlot = i;
			break;
		}
	}
	if (!targetSlot.has_value())
	{
		AVSLOG(Error) << "PipelineNode: Cannot link nodes: no free source slot found from "<<(int)target.getNumInputSlots()<<" slots. Slots linked already:";
		for (int i = 0; i < (int)target.getNumInputSlots(); ++i)
		{
			AVSLOG(Error) << "\tSlot "<<i<<": "<<target.getInput(i)->getDisplayName()<<"\n";
		}
		return Result::Node_LinkFailed;
	}

	return PipelineNode::link(source, *sourceSlot, target, *targetSlot);
}

Result PipelineNode::unlink(PipelineNode& source, int sourceSlot, PipelineNode& target, int targetSlot)
{
	assert(sourceSlot >= 0);
	assert(targetSlot >= 0);

	if (size_t(sourceSlot) >= source.d().m_outputs.size())
	{
		return Result::Node_InvalidSlot;
	}
	if (size_t(targetSlot) >= target.d().m_inputs.size())
	{
		return Result::Node_InvalidSlot;
	}

	Link& sourceOutputLink = source.d().m_outputs[sourceSlot];
	Link& targetInputLink = target.d().m_inputs[targetSlot];

	if (sourceOutputLink.targetNode != &target || sourceOutputLink.targetSlot != targetSlot ||
		targetInputLink.targetNode != &source || targetInputLink.targetSlot != sourceSlot)
	{
		return Result::Node_InvalidLink;
	}

	target.onInputUnlink(targetSlot, &source);
	targetInputLink = {};
	source.onOutputUnlink(sourceSlot, &target);
	sourceOutputLink = {};
	return Result::OK;
}

Result PipelineNode::unlinkInput(int slot)
{
	auto breakLink = [this](int inputSlot, Link& link)
	{
		if (link.targetNode)
		{
			assert(size_t(link.targetSlot) < link.targetNode->d().m_outputs.size());
			onInputUnlink(inputSlot, link.targetNode);
			link.targetNode->onOutputUnlink(link.targetSlot, this);
			link.targetNode->d().m_outputs[link.targetSlot] = {};
		}
		link = {};
	};

	if (slot >= 0)
	{
		if (size_t(slot) >= d().m_inputs.size())
		{
			return Result::Node_InvalidSlot;
		}
		breakLink(slot, d().m_inputs[slot]);
	}
	else {
		for (size_t i = 0; i < d().m_inputs.size(); ++i)
		{
			breakLink(int(i), d().m_inputs[i]);
		}
	}
	return Result::OK;
}

Result PipelineNode::unlinkOutput(int slot)
{
	auto breakLink = [this](int outputSlot, Link& link)
	{
		if (link.targetNode)
		{
			assert(size_t(link.targetSlot) < link.targetNode->d().m_inputs.size());
			onOutputUnlink(outputSlot, link.targetNode);
			link.targetNode->onInputUnlink(link.targetSlot, this);
			link.targetNode->d().m_inputs[link.targetSlot] = {};
		}
		link = {};
	};

	if (slot >= 0)
	{
		if (size_t(slot) >= d().m_outputs.size())
		{
			return Result::Node_InvalidSlot;
		}
		breakLink(slot, d().m_outputs[slot]);
	}
	else {
		for (size_t i = 0; i < d().m_outputs.size(); ++i)
		{
			breakLink(int(i), d().m_outputs[i]);
		}
	}
	return Result::OK;
}

void PipelineNode::unlinkAll()
{
	unlinkInput();
	unlinkOutput();
}

size_t PipelineNode::getNumInputSlots() const
{
	return d().m_inputs.size();
}

size_t PipelineNode::getNumOutputSlots() const
{
	return d().m_outputs.size();
}

Result PipelineNode::isInputLinked(int slot) const
{
	if (slot < 0 || size_t(slot) >= d().m_inputs.size())
	{
		return Result::Node_InvalidSlot;
	}
	return d().m_inputs[slot].targetNode ? Result::OK : Result::Node_NotLinked;
}

Result PipelineNode::isOutputLinked(int slot) const
{
	if (slot < 0 || size_t(slot) >= d().m_outputs.size())
	{
		return Result::Node_InvalidSlot;
	}
	return d().m_outputs[slot].targetNode ? Result::OK : Result::Node_NotLinked;
}

Result PipelineNode::process(uint64_t timestamp, uint64_t deltaTime)
{
#if TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH
	measureBandwidth(deltaTime);
#endif
	return Result::OK;
}

PipelineNode *PipelineNode::getOutput(int slot)
{
	if (slot < 0 || size_t(slot) >= d().m_outputs.size())
	{
		return nullptr;
	}
	return d().m_outputs[slot].targetNode;
}

PipelineNode *PipelineNode::getInput(int slot)
{
	if (slot < 0 || size_t(slot) >= d().m_inputs.size())
	{
		return nullptr;
	}
	return d().m_inputs[slot].targetNode ;
}

const PipelineNode *PipelineNode::getOutput(int slot) const
{
	if (slot < 0 || size_t(slot) >= d().m_outputs.size())
	{
		return nullptr;
	}
	return d().m_outputs[slot].targetNode;
}

const PipelineNode *PipelineNode::getInput(int slot) const
{
	if (slot < 0 || size_t(slot) >= d().m_inputs.size())
	{
		return nullptr;
	}
	return d().m_inputs[slot].targetNode;
}

void PipelineNode::setNumInputSlots(size_t numSlots)
{
	for (size_t slot = numSlots; slot < d().m_inputs.size(); ++slot)
	{
		unlinkInput(static_cast<int>(slot));
	}
	d().m_inputs.resize(numSlots);
}

void PipelineNode::setNumOutputSlots(size_t numSlots)
{
	for (size_t slot = numSlots; slot < d().m_outputs.size(); ++slot)
	{
		unlinkOutput(int(slot));
	}
	d().m_outputs.resize(numSlots);
}

void PipelineNode::setNumSlots(size_t numInputSlots, size_t numOutputSlots)
{
	setNumInputSlots(numInputSlots);
	setNumOutputSlots(numOutputSlots);
}


int PipelineNode::getInputIndex(const PipelineNode* node) const
{
	assert(node);
	for (size_t i = 0; i < d().m_inputs.size(); ++i)
	{
		if (d().m_inputs[i].targetNode == node)
		{
			return int(i);
		}
	}
	return -1;
}

int PipelineNode::getOutputIndex(const PipelineNode* node) const
{
	assert(node);
	for (size_t i = 0; i < d().m_outputs.size(); ++i)
	{
		if (d().m_outputs[i].targetNode == node)
		{
			return int(i);
		}
	}
	return -1;
}

#if TELEPORT_LIBAV_MEASURE_PIPELINE_BANDWIDTH
void PipelineNode::measureBandwidth(uint64_t deltaTime)
{
	if(deltaTime==0)
		return;
	static float intro=0.01f;
	inwardBandwidthKps*=1.0f-intro;
	inwardBandwidthKps+=intro*(float)bytes_received.load()/float(deltaTime)*(1000.0f/1024.0f);
	bytes_received= 0;
}
#endif