LibAVStream
===========

Introduction
------------

LibAVStream is a helper library for streaming video and geometry data between a server and client. A user of this library constructs an avs::Pipeline instance, and configures the pipeline with avs::PipelineNode subclass instances. Nodes can receive data from other nodes, process it, and pass the processed data on to other nodes.

A pipeline operates on a single thread, while queues allow threads to exchange data. For example, the server's Network pipeline connects several avs::Queue instances to a single avs::NetworkSink. For example, the video encoding pipeline links an avs::Surface that receives raw video frames, to an avs::Encoder, and finally to a queue. On another thread, the same queue in the network pipeline passes the data to the avs::NetworkSink.

Classes
-------

.. doxygenclass:: avs::Context
:members:

.. doxygenclass:: avs.Context
	:members:

.. doxygenclass:: Context
	:members:

.. doxygenclass:: avs::Pipeline
.. doxygenclass:: avs::Timer
.. doxygenclass:: avs::UseInternalAllocator

avs Interface Classes
---------------------
.. doxygenclass:: avs::AudioTargetBackendInterface
.. doxygenclass:: avs::AudioEncoderBackendInterface
.. doxygenclass:: avs::IOInterface
.. doxygenclass:: avs::PacketInterface
.. doxygenclass:: avs::SurfaceInterface
.. doxygenclass:: avs::GeometrySourceInterface
.. doxygenclass:: avs::GeometryTargetInterface
.. doxygenclass:: avs::AudioTargetInterface

avs Nodes
---------
.. doxygenclass:: avs::PipelineNode

.. doxygenclass:: avs::AudioDecoder
.. doxygenclass:: avs::AudioEncoder
.. doxygenclass:: avs::Buffer
.. doxygenclass:: avs::Decoder
.. doxygenclass:: avs::Encoder
.. doxygenclass:: avs::File
.. doxygenclass:: avs::Forwarder
.. doxygenclass:: avs::GeometryDecoder
.. doxygenclass:: avs::GeometryEncoder
.. doxygenclass:: avs::GeometrySource
.. doxygenclass:: avs::GeometryTarget
.. doxygenclass:: avs::NetworkSink
.. doxygenclass:: avs::NetworkSource
.. doxygenclass:: avs::NullSink
.. doxygenclass:: avs::Packetizer
.. doxygenclass:: avs::Queue
.. doxygenclass:: avs::Surface
.. doxygenclass:: avs::TagDataDecoder
:members:

avs Structs
-----------
.. doxygenstruct:: avs::ClientMessage
.. doxygenstruct:: avs::NetworkSinkCounters
.. doxygenstruct:: avs::NetworkSinkParams
.. doxygenstruct:: avs::NetworkSinkStream
.. doxygenstruct:: avs::NetworkSourceCounters
.. doxygenstruct:: avs::NetworkSourceParams
.. doxygenstruct:: avs::NetworkSourceStream
.. doxygenstruct:: avs::Result
   :members:
