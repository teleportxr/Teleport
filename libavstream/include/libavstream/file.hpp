// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>

namespace avs
{

/* File access mode */
enum class FileAccess
{
	None = 0, /*!< Unconfigured. */
	Read,     /*!< Read only access. */
	Write,    /*!< Write only access. */
};

/*!
 * File node `[passive, 1/1]`
 *
 * File node provides a way to read or write from binary files and thus can act
 * either as a data sink or data source depending on configured FileAccess mode.
 */
class AVSTREAM_API File final : public PipelineNode
	                          , public IOInterface
	                          , public PacketInterface
{
	AVSTREAM_PUBLICINTERFACE(File)
public:
	File();

	/*!
	 * Configure file node.
	 * \param filename Filename of source or destination file.
	 * \param access File access mode:
	 *               - If FileAccess::Read then node is configured as data source and can only be read from.
	 *               - If FileAccess::Write then node is configured as data sink and can only be written to.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_AlreadyConfigured if file node has already been configured.
	 *  - Result::Node_InvalidConfiguration if access is neither FileAccess::Read nor FileAccess::Write.
	 *  - Result::File_OpenFailed if failed to open source/destination file.
	 */
	Result configure(const char* filename, FileAccess access);

	/*!
	 * Deconfigure file node and close source/destination file.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if file node has not been configured.
	 */
	Result deconfigure() override;

	/*!
	 * Read bytes from file.
	 * \sa IOInterface::read()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if file node has not been configured.
	 *  - Result::IO_Retry if bufferSize is zero.
	 *  - Result::File_ReadFailed if access mode is not FileAccess::Read or read failure occured.
	 *  - Result::File_EOF if reached the end of file.
	 */
	Result read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead) override;

	/*!
	 * Write bytes to file.
	 * \sa IOInterface::write()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if file node has not been configured.
	 *  - Result::File_WriteFailed if access mode is not FileAccess::Write or write failure occured.
	 */
	Result write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;

	/*!
	 * File node does not support packet read operations.
	 * \return Always returns Result::Node_NotSupported.
	 */
	Result readPacket(PipelineNode* reader, void* buffer, size_t& bufferSize, const int streamIndex) override;

	/*!
	 * Write packet to file.
	 * \sa PacketInterface::writePacket()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if file node has not been configured.
	 *  - Result::File_WriteFailed if access mode is not FileAccess::Write or write failure occured.
	 */
	Result writePacket(PipelineNode* writer, const void* buffer, size_t bufferSize,int streamIndex) override;
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "File"; }

	/*! Get file access mode */
	FileAccess getAccessMode() const;
	/*! Get file name */
	const char* getFileName() const;
};

} // avs