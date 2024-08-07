// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/interfaces.hpp>

#ifndef FIX_BROKEN
#include <../src/abi_p.hpp>

#include <vector>

namespace avs
{
	/*!
	 * Abstract processing node.
	 *
	 * Processing nodes are fundamental building blocks of pipelines.
	 * Each node can have N input slots, and M output slots (N,M >= 0), and can do some amount of work during pipeline processing.
	 *
	 * Nodes can be classified as "input-active" and/or "output-active", or "passive".
	 * - Input-Active nodes read data from their inputs during processing.
	 * - Output-Active nodes write data to their outputs during processing.
	 * - Passive nodes usually don't do any processing and just act as data sources & sinks to other, active, nodes.
	 *
	 * Being aware of this classification is very important in correctly constructing pipelines.
	 */
	class AVSTREAM_API PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE_BASE(PipelineNode)
	public:
		PipelineNode(const PipelineNode&) = delete;
		PipelineNode(PipelineNode&&) = delete;
		virtual ~PipelineNode();
		std::string name;
		float inwardBandwidthKps=0.f;
		float maxPacketKb=0.f;
		/*!
		 * Link two nodes. Data will flow from source node to target node.
		 * \param source Source node.
		 * \param sourceSlot Index of source node output slot.
		 * \param target Target node.
		 * \param targetSlot Index of target node input slot.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidSlot if sourceSlot is not valid source node output slot index.
		 *  - Result::Node_InvalidSlot if targetSlot is not valid target node input slot index.
		 *  - Any error returned by source node onOutputLink() function.
		 *  - Any error returned by target node onInputLink() function.
		 */
		static Result link(PipelineNode& source, int sourceSlot, PipelineNode& target, int targetSlot);

		/*!
		 * Link two nodes. Data will flow from source node to target node.
		 * First available source and target slot will be picked automatically to make the link.
		 * \param source Source node.
		 * \param target Target node.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_LinkFailed if couldn't find any free source or target slot to make the link.
		 *  - Any error returned by source node onOutputLink() function.
		 *  - Any error returned by target node onInputLink() function.
		 */
		static Result link(PipelineNode& source, PipelineNode& target);

		/*!
		 * Unlink two nodes.
		 * \param source Source node.
		 * \param sourceSlot Index of source node output slot.
		 * \param target Target node.
		 * \param targetSlot Index of target node input slot.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidSlot if sourceSlot is not valid source node output slot index.
		 *  - Result::Node_InvalidSlot if targetSlot is not valid target node input slot index.
		 *  - Result::Node_InvalidLink if source and target nodes were not linked.
		 */
		static Result unlink(PipelineNode& source, int sourceSlot, PipelineNode& target, int targetSlot);

		/*!
		 * Unlink node by input slot index.
		 * \param slot Input slot index or -1 to unlink all inputs.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidSlot if slot is not valid input slot index.
		 */
		Result unlinkInput(int slot = -1);

		/*!
		 * Unlink node by output slot index.
		 * \param slot Output slot index or -1 to unlink all outputs.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidSlot if slot is not valid output slot index.
		 */
		Result unlinkOutput(int slot = -1);

		/*!
		 * Unlink all inputs and outputs.
		 */
		void unlinkAll();

		/*! Get number of input slots. */
		size_t getNumInputSlots() const;
		/*! Get number of output slots. */
		size_t getNumOutputSlots() const;

		/*!
		 * Query whether another node is linked to a particular input slot.
		 * \param slot Input slot index.
		 * \return
		 *  - Result::OK if a node is linked to this input slot.
		 *  - Result::Node_NotLinked if no node is linked to this input slot.
		 *  - Result::Node_InvalidSlot if slot is is not valid input slot index.
		 */
		Result isInputLinked(int slot) const;

		/*!
		 * Query whether another node is linked to a particular output slot.
		 * \param slot Output slot index.
		 * \return
		 *  - Result::OK if a node is linked to this output slot.
		 *  - Result::Node_NotLinked if no node is linked to this output slot.
		 *  - Result::Node_InvalidSlot if slot is is not valid output slot index.
		 */
		Result isOutputLinked(int slot) const;

		/*!
		 * Deconfigure this node.
		 * \return
		 *  - Result::OK on success.
		 *  - PipelineNode specific error result on failure.
		 */
		virtual Result deconfigure()
		{
			return Result::OK;
		}

		/*!
		 * Perform any node specific work as part of a pipeline.
		 * \param timestamp Pipeline timestamp.
		 * \param deltaTime Pipeline timestamp.
		 * \return
		 *  - Result::OK on success (or no-op).
		 *  - PipelineNode specific error result on failure.
		 */
		virtual Result process(uint64_t timestamp, uint64_t deltaTime);

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		virtual const char* getDisplayName() const = 0;

		
		/*!
		 * Get the output node at the given slot.
		 */
		PipelineNode *getOutput(int slot);
		const PipelineNode *getOutput(int slot) const;
		/*!
		 * Get the input node at the given slot.
		 */
		PipelineNode *getInput(int slot);
		const PipelineNode *getInput(int slot) const;

		void setResult(Result r)
		{
			lastResult=r;
		}
		Result getLastResult() const
		{
			return lastResult;
		}
	protected:
		/*! Configure number of input slots for this node. */
		void setNumInputSlots(size_t numSlots);
		/*! Configure number of output slots for this node. */
		void setNumOutputSlots(size_t numSlots);
		/*! Configure number of input and output slots for this node. */
		void setNumSlots(size_t numInputSlots, size_t numOutputSlots);

		/*! Get index of input slot a given node is linked to; returns -1 if given node is not linked to any input slot. */
		int getInputIndex(const PipelineNode* node) const;
		/*! Get index of output slot a given node is linked to; returns -1 if given node is not linked to any input slot. */
		int getOutputIndex(const PipelineNode* node) const;
		
		std::atomic<size_t> bytes_received=0;
		void measureBandwidth(uint64_t deltaTime);
	private:
		Result lastResult = avs::Result::OK;
		/*!
		 * Input link event: Called when a node is about to be linked to an input slot.
		 * \param slot Input slot index.
		 * \param node PipelineNode about to be linked.
		 * \return Result other than Result::OK aborts linking.
		 */
		virtual Result onInputLink(int slot, PipelineNode* node) { return Result::OK; }

		/*!
		 * Output link event: Called when a node is about to be linked to an output slot.
		 * \param slot Output slot index.
		 * \param node PipelineNode about to be linked.
		 * \return Result other than Result::OK aborts linking.
		 */
		virtual Result onOutputLink(int slot, PipelineNode* node)
		{
			return Result::OK;
		}

		/*!
		 * Input unlink event: Called when a node is about to be unlinked from an input slot.
		 * \param slot Input slot index.
		 * \param node PipelineNode about to be unlinked.
		 */
		virtual void onInputUnlink(int slot, PipelineNode* node) {}

		/*!
		 * Output unlink event: Called when a node is about to be unlinked from an output slot.
		 * \param slot Output slot index.
		 * \param node PipelineNode about to be unlinked.
		 */
		virtual void onOutputUnlink(int slot, PipelineNode* node)
		{}
	};
	
	struct Link
	{
		PipelineNode* targetNode = nullptr;
		int   targetSlot = 0;

		operator bool() const { return targetNode != nullptr; }
	};

	struct PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE_BASE(PipelineNode)
		virtual ~Private() = default;

		std::vector<Link> m_inputs;
		std::vector<Link> m_outputs;
	};
} // avs
#endif