// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <../src/platform.hpp>
#include <initializer_list>
#include <fstream>
#include <thread>


namespace avs
{
class PipelineNode;

/*!
 * A pipeline.
 *
 * A pipeline is a collection of nodes linked together in a serial manner such that each node produces output for the next node.
 * A pipeline does not take ownership of its nodes; a single node can be a member of more than one pipeline.
 *
 * It is recommended to create no more than a single pipeline per application thread and link pipelines via
 * shared Queue or Buffer nodes to ensure thread safety.
 */
class AVSTREAM_API Pipeline final
{
	AVSTREAM_COMPLETEINTERFACE_FINAL(Pipeline)
	
	std::vector<PipelineNode*> m_nodes;
	Timestamp m_startPlatformTimestamp;
	uint64_t m_lastTimestamp;
	uint64_t m_startTimestamp;
	bool m_started = false;

	std::ofstream m_statFile;
	void writeTimingsHeader();
	void writeTimings(uint32_t timestamp, const std::vector<double>& timings);

	bool m_blocked = false;
	std::thread pipelineThread;
	std::atomic<bool> pipelineThreadActive=false;
	void processAsyncFn();
public:
	Pipeline();
	~Pipeline();

	void SetPipelineBlocked(bool b)
	{
		m_blocked = b;
	}
	bool IsPipelineBlocked() const
	{
		return m_blocked;
	}

	uint64_t GetStartTimestamp() const;
	uint64_t GetTimestamp() const;
	/*!
	 * Add node to the end of the pipeline.
	 */
	void add(PipelineNode* node);

	//! Add multiple nodes to the end of the pipeline in the order specified.
	void add(const std::initializer_list<PipelineNode*>& nodes);

	/*!
	 * Link multiple nodes and add them at the end of the pipeline.
	 * The first added node is *not* automatically linked with the node previously at the end of the pipeline.
	 * This function can be used to build whole pipelines in a single function call.
	 * Returns
	 *  Result::OK on success.
	 *  Any error result returned by PipelineNode::link() function.
	 * 
	 * See also Pipeline::add()
	 */
	Result link(const std::initializer_list<PipelineNode*>& nodes);

	/*!
	 * Get node at the front of the pipeline; returns nullptr if pipeline is empty.
	 */
	PipelineNode* front() const;

	/*!
	 * Get node at the back (end) of the pipeline; returns nullptr if pipeline is empty.
	 */
	PipelineNode *back() const;

	const std::vector<PipelineNode *> &getNodes() const
	{
		return m_nodes;
	}

	/*!
	 * Get pipeline length (number of nodes in the pipeline).
	 */
	size_t getLength() const;

	/*!
	 * Process pipeline.
	 * Calls PipelineNode::process() on each node in pipeline order providing consistent timestamp to each invocation.
	 * Returns:
	 *  - Result::OK on success.
	 *  - An error result returned by a failing node (pipeline processing is immediately aborted on first PipelineNode::process() failure).
	 *
	 * Pipeline time starts when process() is called first and is backed up by platform's high resolution monotonic clock.
	 */
	Result process();
	
	//! @brief Process continuously on a thread until stopped.
	void processAsync();

	/*!
	 * Deconfigure all nodes in the pipeline.
	 */
	void deconfigure();

	/*!
	 * Reset pipeline.
	 * Unlinks all nodes and removes them from the pipeline.
	 */
	void reset();

	/*!
	 * Restart pipeline.
	 * Pipeline time counter will be reinitialized at next call to Pipeline::process().
	 */
	void restart();

	/*!
	 * Start profiling the pipeline.
	 * \param statFileName Path to output CSV file with resultant timings.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::File_OpenFailed if output file coult not be opened for writing.
	 *  - Result::Pipeline_AlreadyProfiling if pipeline profile has already been started.
	 */
	Result startProfiling(const char* statFileName);

	/*!
	 * Stop profiling the pipeline.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Pipeline_NotProfiling if pipeline profile has not been started.
	 */
	Result stopProfiling();
};

} // avs