LibAVStream
===========

Introduction
------------

LibAVStream is a helper library for streaming video and geometry data between a server and client. A user of this library constructs an avs::Pipeline instance, and configures the pipeline with avs::PipelineNode subclass instances. Nodes can receive data from other nodes, process it, and pass the processed data on to other nodes.

A pipeline operates on a single thread, while queues allow threads to exchange data. For example, the server's Network pipeline connects several avs::Queue instances to a single avs::NetworkSink. For example, the video encoding pipeline links an avs::Surface that receives raw video frames, to an avs::Encoder, and finally to a queue. On another thread, the same queue in the network pipeline passes the data to the avs::NetworkSink.

LibAVStream EFP over WebRTC for data stream reliability and deliverability.

.. figure:: /images/reference/ExamplePipeline.png
	:width: 800
	:alt: Two pipelines operating on separate threads are connected using an avs::Queue.

	Two pipelines operating on separate threads are connected using an avs::Queue.

Namespace
---------
.. doxygennamespace :: avs

