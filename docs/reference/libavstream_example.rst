LibAVStream Example
===================

Introduction
------------

Here we present an example of how to use libavstream to stream video.
LibAVStream has a default video encoder and decoder class that uses NVENC but for this example, we will assume a custom 
video encoder needs to be used.


Video Stream Example
-----------------------
avs::NetworkSink is an avs::PipelineNode that handles the packing and sending of data for each stream.
An avs::NetworkSink instance must be created and configured. Each stream is configured by passing an instance of 
an avs::NetworkStream struct to the avs::NetworkSink configure function. 
An avs::Pipeline is used to send data between multiple linked avs::PipelineNode instances.
It is useful to create two avs::Pipeline instances. One that processes the encoding and queueing of data 
on one thread and another that processes the dequeueing and sending of data on another thread.
The encoder will write data to the avs::IOInterface(s) and the network sink will read data from the avs::IOInterface(s). 
There must be a separate avs::IOInterface instance for each stream. The avs::Queue class derives avs::IOInterface and 
avs::PipelineNode and is useful for the asynchronous writing and reading of data. The queue must be linked to the video 
encoder as an output node and to the network sink as an input node. The node implementing the avs::IOInterface is therefore
the only node that will be used on the encoder and network transfer pipelines.

This example will assume the video texture to be encoded was created using the D3D12 API. Currently LibAVStream supports
D3D11 abd D3D12 but this can easily be extended.

The example below shows how to create and configure an avs::Queue.
``
avs::Queue* createVideoQueue()
{
	avs::Queue* videoQueue = new avs::Queue();
	size_t maxBufferSize = 200000;
	size_t maxBuffers = 16;
	videoQueue->configure(maxBufferSize, maxBuffers,"VideoQueue");
	return videoQueue;
}
``

The example below demonstrates how to create the video encoding pipeline in a wrapper class:

``
class VideoEncodingPipeline
{
public:
	void Initialize(ID3D12Device* device, ID3D12Resource* videoTexture, avs::EncoderBackendInterface* customVideoEncoder, avs::Queue& videoQueue)
	{
		avs::SurfaceBackendInterface* avsSurfaceBackend = new avs::SurfaceDX12(videoTexture);

		if (!mInputSurface.configure(avsSurfaceBackend))
		{
			// handle error
			return;
		}

		avs::DeviceHandle deviceHandle;
		deviceHandle.type = avs::DeviceType::Direct3D12;
		deviceHandle.handle = device;

		D3D12_RESOURCE_DESC desc = videoTexture->GetDesc();

		avs::EncoderPrams encoderParams;
		encoderParams.codec = avs::VideoCodec::HEVC;
		encoderParams.preset = avs::VideoPreset::HighPerformance;
		encoderParams.targetFrameRate = 60;
		encoderParams.idrInterval = 60;
		encoderParams.rateControlMode = avs::RateControlMode::RC_CBR;
		encoderParams.averageBitrate = 40000000;
		encoderParams.maxBitrate = 80000000;
		encoderParams.autoBitRate = false;
		encoderParams.vbvBufferSizeInFrames = 3;
		encoderParams.deferOutput = false;
		encoderParams.useAsyncEncoding = true;
		encoderParams.use10BitEncoding = false;
		encoderParams.useAlphaLayerEncoding = false;
		encoderParams.inputFormat = avs::SurfaceFormat::ARGB;

		// The custom encoder backend must be set before configure is called.
		// If not set, the avs::EncoderNV backend will be used instead.
		mEncoder.setBackend(customVideoEncoder);

		if (!mEncoder.configure(deviceHandle, desc.Width, desc.Height, encoderParams))
		{
			// handle error
			return;
		}

		if (!mPipeline.link({ &mInputSurface, &mEncoder, &videoQueue }))
		{
			// handle error
			return;
		}
	}

	// Call on render thread.
	void process()
	{
		// Read the data from the queue and send it to the client.
		if(!mPipeline.process())
		{
			// handle error
		}
	}

	void shutdown()
	{
		// Calls deconfigure on all nodes.
		if(!mPipeline.deconfigure())
		{
			// handle error
		}
	}

private:
	// Class members
	avs::Pipeline mPipeline;
	avs::Encoder mEncoder;
	avs::Surface mInputSurface;
}	
``


The example below illustrates how to create a network transfer pipeline in a wrapper class:

``
class NetworkPipeline
{
public:
	void Initialize(avs::Queue& videoQueue, const char* remoteIP, uint16_t remotePort)
	{
		avs::NetworkSinkParams sinkParams;
		params.connectionTimeout = 5000.

		std::vector<avs::NetworkSinkStream> streams;

		avs::NetworkSinkStream stream;
		stream.parserType = avs::StreamParserType::AVC_AnnexB;
		stream.useParser = false;
		stream.isDataLimitPerFrame = false;
		stream.counter = 0;
		stream.chunkSize = 64 * 1024;
		stream.id = 20;
		stream.dataType = avs::NetworkDataType::HEVC;
		streams.emplace_back(std::move(stream));
	
		if (!mNetworkSink.configure(std::move(streams), nullptr, 0, remoteIP, remotePort, sinkParams))
		{
			// handle error
			return;
		}

		// Link video queue as input node for the network sink.
		if (!mPipeline.link({ &videoQueue, &mNetworkSink }))
		{
			// handle error
			return;
		}
	}

	// Call on network thread.
	void process()
	{
		// Read the data from the queue and send it to the client.
		if(!mPipeline.process())
		{
			// handle error
		}
	}

	void shutdown()
	{
		// Calls deconfigure on all nodes.
		if(!mPipeline.deconfigure())
		{
			// handle error
		}
	}

private:
	// Class members
	avs::Pipeline mPipeline;
	avs::NetworkSink mNetworkSink;
}
``


