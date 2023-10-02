LibAVStream
===========

Introduction
------------

LibAVStream is a helper library for streaming video and geometry data between a server and client. A user of this library constructs an avs::Pipeline instance, and configures the pipeline with avs::PipelineNode subclass instances. Nodes can receive data from other nodes, process it, and pass the processed data on to other nodes.

A pipeline operates on a single thread, while queues allow threads to exchange data. For example, the server's Network pipeline connects several avs::Queue instances to a single avs::NetworkSink. For example, the video encoding pipeline links an avs::Surface that receives raw video frames, to an avs::Encoder, and finally to a queue. On another thread, the same queue in the network pipeline passes the data to the avs::NetworkSink.

LibAVStream uses Enet for UDP datagram control. It uses SRT and EFP over UDP for data stream reliability and deliverability.

.. figure:: /images/reference/ExamplePipeline.png
	:width: 800
	:alt: Two pipelines operating on separate threads are connected using an avs::Queue.

	Two pipelines operating on separate threads are connected using an avs::Queue.

Classes
-------


.. doxygenclass:: avs::Context
	:members:

.. doxygenclass:: avs::Pipeline
	:members:

.. doxygenclass:: avs::Timer
	:members:

.. doxygenclass:: avs::UseInternalAllocator
	:members:

avs Interface Classes
---------------------
.. doxygenclass:: avs::AudioTargetBackendInterface
   :members:
.. doxygenclass:: avs::AudioEncoderBackendInterface
   :members:
.. doxygenclass:: avs::IOInterface
   :members:
.. doxygenclass:: avs::PacketInterface
   :members:
.. doxygenclass:: avs::SurfaceInterface
   :members:
.. doxygenclass:: avs::GeometrySourceInterface
   :members:
.. doxygenclass:: avs::GeometryTargetInterface
   :members:
.. doxygenclass:: avs::AudioTargetInterface
   :members:
.. doxygenclass:: avs::EncoderBackendInterface
   :members:
.. doxygenclass:: avs::DecoderBackendInterface
   :members:

avs Nodes
---------
.. doxygenclass:: avs::PipelineNode
   :members:
.. doxygenclass:: avs::AudioDecoder
   :members:
.. doxygenclass:: avs::AudioEncoder
   :members:
.. doxygenclass:: avs::Buffer
.. doxygenclass:: avs::Decoder
   :members:
.. doxygenclass:: avs::Encoder
   :members:
.. doxygenclass:: avs::File
.. doxygenclass:: avs::Forwarder
.. doxygenclass:: avs::GenericDecoder
   :members:
.. doxygenclass:: avs::GenericEncoder
   :members:
.. doxygenclass:: avs::GeometryDecoder
   :members:
.. doxygenclass:: avs::GeometryEncoder
   :members:
.. doxygenclass:: avs::GeometrySource
   :members:
.. doxygenclass:: avs::GeometryTarget
   :members:
.. doxygenclass:: avs::NetworkSink
   :members:
.. doxygenclass:: avs::NetworkSource
   :members:
.. doxygenclass:: avs::NullSink
.. doxygenclass:: avs::Packetizer
.. doxygenclass:: avs::Queue
   :members:
.. doxygenclass:: avs::Surface
   :members:
.. doxygenclass:: avs::TagDataDecoder
   :members:

avs Structs
-----------
.. doxygenstruct:: avs::NetworkSinkCounters
   :members:
.. doxygenstruct:: avs::NetworkSinkParams
   :members:
.. doxygenstruct:: avs::NetworkSinkStream
   :members:
.. doxygenstruct:: avs::NetworkSourceCounters
   :members:
.. doxygenstruct:: avs::NetworkSourceParams
   :members:
.. doxygenstruct:: avs::NetworkSourceStream
   :members:
.. doxygenstruct:: avs::Result
   :members:
   