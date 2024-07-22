// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include "file_p.hpp"

namespace avs {

	File::File()
		: PipelineNode(new File::Private(this))
	{}

	Result File::configure(const char* filename, FileAccess access)
	{
		if (d().m_file.is_open())
		{
			return Result::Node_AlreadyConfigured;
		}

		int openMode = std::ios::binary;
		switch (access)
		{
		case FileAccess::Read:
			setNumOutputSlots(1);
			openMode |= std::ios::in;
			break;
		case FileAccess::Write:
			setNumInputSlots(1);
			openMode |= std::ios::out | std::ios::trunc;
			break;
		default:
			return Result::Node_InvalidConfiguration;
		}

		d().m_file.open(filename, (std::ios_base::openmode)openMode);
		if (!d().m_file.is_open())
		{
			setNumSlots(0, 0);
			AVSLOG(Error) << "File: Could not open file: " << filename;
			return Result::File_OpenFailed;
		}

		d().m_filename = filename;
		d().m_access = access;
		return Result::OK;
	}

	Result File::deconfigure()
	{
		if (!d().m_file.is_open())
		{
			return Result::Node_NotConfigured;
		}

		setNumSlots(0, 0);

		d().m_file.close();
		d().m_filename = "";
		d().m_access = FileAccess::None;
		return Result::OK;
	}

	Result File::read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead)
	{
		if (!d().m_file.is_open())
		{
			return Result::Node_NotConfigured;
		}
		if (d().m_access != FileAccess::Read)
		{
			return Result::File_ReadFailed;
		}
		if (bufferSize == 0)
		{
			return Result::IO_Retry;
		}

		d().m_file.read(static_cast<char*>(buffer), bufferSize);
		bytesRead = static_cast<size_t>(d().m_file.gcount());
		if (d().m_file.eof())
		{
			return Result::File_EOF;
		}
		if (!d().m_file)
		{
			return Result::File_ReadFailed;
		}
		return Result::OK;
	}

	Result File::write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		if (!d().m_file.is_open())
		{
			return Result::Node_NotConfigured;
		}
		if (d().m_access != FileAccess::Write)
		{
			return Result::File_WriteFailed;
		}

		auto beforeWritePosition = d().m_file.tellp();
		d().m_file.write(static_cast<const char*>(buffer), bufferSize);
		bytesWritten = static_cast<size_t>(d().m_file.tellp() - beforeWritePosition);
		if (!d().m_file)
		{
			return Result::File_WriteFailed;
		}
		return Result::OK;
	}

	Result File::readPacket(PipelineNode* reader, void* buffer, size_t& bufferSize, int streamId)
	{
		AVSLOG(Warning) << "File: Packet read is not supported";
		return Result::Node_NotSupported;
	}

	Result File::writePacket(PipelineNode* writer, const void* buffer, size_t bufferSize, int streamIndex)
	{
		if (!d().m_file.is_open())
		{
			return Result::Node_NotConfigured;
		}
		if (d().m_access != FileAccess::Write)
		{
			return Result::File_WriteFailed;
		}

		const uint32_t packetSize = static_cast<uint32_t>(bufferSize);
		d().m_file.write(reinterpret_cast<const char*>(&packetSize), sizeof(packetSize));
		d().m_file.write(static_cast<const char*>(buffer), bufferSize);
		if (!d().m_file)
		{
			return Result::File_WriteFailed;
		}
		return Result::OK;
	}

	FileAccess File::getAccessMode() const
	{
		return d().m_access;
	}

	const char* File::getFileName() const
	{
		return d().m_filename.c_str();
	}

} // avs