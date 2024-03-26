The Teleport Core library
==========================

Introduction
------------

Teleport Core is a library of fundamental classes shared by clients and servers.

Commands
--------

Commands are sent from the server to the client on a reliable data channel. All commands derive from teleport::core::Command.

.. doxygenstruct:: teleport::core::Command
	:members:

.. doxygenstruct:: teleport::core::SetStageSpaceOriginNodeCommand
	:members:

.. doxygenstruct:: teleport::core::AcknowledgeHandshakeCommand
	:members:

.. doxygenstruct:: teleport::core::SetupCommand
	:members:

.. doxygenstruct:: teleport::core::SetupLightingCommand
	:members:

.. doxygenstruct:: teleport::core::SetupInputsCommand
	:members:

.. doxygenstruct:: teleport::core::ReconfigureVideoCommand
	:members:

.. doxygenstruct:: teleport::core::ShutdownCommand
	:members:

.. doxygenstruct:: teleport::core::NodeVisibilityCommand
	:members:

.. doxygenstruct:: teleport::core::UpdateNodeMovementCommand
	:members:

.. doxygenstruct:: teleport::core::UpdateNodeEnabledStateCommand
	:members:

.. doxygenstruct:: teleport::core::SetNodeHighlightedCommand
	:members:

.. doxygenstruct:: teleport::core::UpdateNodeStructureCommand
	:members:

.. doxygenstruct:: teleport::core::AssignNodePosePathCommand
	:members:

Client Messages
---------------
.. doxygenstruct:: teleport::core::ClientMessage
   :members:
.. doxygenstruct:: avs::DisplayInfo
   :members:
.. doxygenstruct:: teleport::core::NodeStatusMessage
   :members:
.. doxygenstruct:: teleport::core::ReceivedResourcesMessage
   :members:
.. doxygenstruct:: teleport::core::ControllerPosesMessage
   :members:

Animation
---------

.. doxygenstruct:: teleport::core::Animation
	:members:

.. doxygenstruct:: teleport::core::TransformKeyframeList
	:members:
	
Input and control
-----------------

.. doxygenclass:: teleport::core::Input
	:members:

Text and Fonts
--------------

.. doxygenstruct:: teleport::core::FontAtlas
	:members:

.. doxygenstruct:: teleport::core::TextCanvas
	:members:
