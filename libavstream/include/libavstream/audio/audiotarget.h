// (C) Copyright 2018-2020 Simul Software Ltd
#pragma once

#include <libavstream/node.hpp>
#include <libavstream/interfaces.hpp>
#include <libavstream/audio/audio_interface.h>

namespace avs
{
	/*! A class to receive and process streamed audio
	*/
	class AVSTREAM_API AudioTarget final : public PipelineNode, public avs::AudioTargetInterface
	{
		AVSTREAM_PUBLICINTERFACE(AudioStreamTarget)
	public:
		AudioTarget();

		~AudioTarget();

	   /*!
		* Configure audio target node.
		* \param backend backend associated with this node.
		* \return
		*  - Result::OK on success.
		*  - Result::Node_AlreadyConfigured if has already been configured with a backend.
		*  - Result::Audio_InvalidBackend if backend is nullptr.
		*/
		Result configure(AudioTargetBackendInterface* audioBackend);

		/*!
		* Returns audio backend interface belonging to this node
		* \return
		*  - AudioTargetBackendInterface::Audio backend interface.
		*/
		AudioTargetBackendInterface* getAudioTargetBackendInterface() const override;

		/*!
		 * Deconfigure node and release its backend.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if node has not been configured.
		 */
		Result deconfigure() override;

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "Audio Target"; }
	};
}

