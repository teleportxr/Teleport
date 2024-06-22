#include "libavstream/audio/audiotarget.h"
#include "common_p.hpp"
#include "node_p.hpp"

namespace avs
{
	struct AudioTarget::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(AudioTarget, PipelineNode);
		AudioTargetBackendInterface* m_backend = nullptr;
	};

	AudioTarget::AudioTarget()
		: avs::PipelineNode(new AudioTarget::Private(this)) {}

	AudioTarget::~AudioTarget() 
	{
		
	}

	AudioTargetBackendInterface* AudioTarget::getAudioTargetBackendInterface() const
	{
		return d().m_backend;
	}

	Result AudioTarget::configure(AudioTargetBackendInterface* audioBackend)
	{
		if (d().m_backend)
		{
			Result deconf_res = deconfigure();
			if (deconf_res != Result::OK)
				return Result::Node_AlreadyConfigured;
		}

		if (!audioBackend)
		{
			return Result::AudioTarget_InvalidBackend;
		}
		name="Audio Target";
		d().m_backend = audioBackend;
		setNumInputSlots(1);

		return Result::OK;
	}

	Result AudioTarget::deconfigure()
	{
		if (!d().m_backend)
		{
			return Result::Node_NotConfigured;
		}

		d().m_backend = nullptr;
		setNumSlots(0, 0);

		return Result::OK;
	}
}
