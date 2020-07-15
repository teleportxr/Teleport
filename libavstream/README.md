Low latency audio/video streaming library for use in games and other soft realtime applications.

# Design goals

- **High performance**  
  libavstream's focus is on supporting hardware video encoders & decoders exclusively in order to offer best possible performance.
  
- **Low latency**  
  Network protocol used for streaming is explicitly designed to be low overhead and low latency.  
  Supported hardware encoders are configured in a way to minimize encoding latency (for example: no B-frames are ever generated when encoding to H264 or HEVC).

- **Explicit threading**  
  Unlike other video procesing libraries libavstream never creates any internal threads. All processing is being done in application controlled event loops
  and application created threads. This is well suited for game engines and makes it easier to properly synchronize video processing with application's own rendering.

- **Deterministic memory management**  
  Most of required memory is allocated upfront during pipeline configuration. The amount of resources needed during processing is bounded and deterministic, and often backed
  by custom allocators to minimize memory fragmentation & operating system allocator overhead.  
  (*We're not there yet but work is being done to fully achieve this goal*)

- **Compiler & toolchain compatibility**  
  Since libavstream is likely to be used with diverse set of compilers and toolchains (PC, Android, Console SDKs, etc.) extra care is being taken to ensure
  binary & C++ standard library compatibility between libavstream and application code. This is achieved through judicious use of PIMPL idiom which allows to ensure
  that publicly exposed API is minimal and does not require the application to link to compatible STL library.

- **Extensibility**  
  libavstream can be easily extended by implementing application specific backends or even implementing custom nodes, if need arises.
  
# Basic concepts

### Nodes

Processing nodes are fundamental building blocks of pipelines. Each node can have some number of input and output slots, and can do some amount of work during pipeline processing.

Nodes can be classified as *input-active* and/or *output-active*, or *passive*.

- **Input-Active** nodes read data from their inputs during processing.
- **Output-Active** nodes write data to their outputs during processing.
- **Passive** nodes usually don't do any processing and just act as data sources & sinks to other, active, nodes.

Being aware of this classification is very important in order to correctly construct pipelines.

### Pipelines

A pipeline is a collection of nodes linked together in a serial manner such that each node produces output for the next node.
Pipelines lie at the heart of libavstream audio/video processing. An application needs at least one pipeline to do anything useful.

### Backends

Backends provide nodes with some specific functionality implementation.  
For example an `avs::Encoder` node might use a particular encoder backend (which implements `avs::EncoderBackendInterface`) to make use of NVIDIA hardware video encoder.
On the other hand `avs::Surface` node might use `avs::SurfaceDX11` backend to provide a Direct3D 11 texture object as a usable input to said encoder node.

# Building

Building requirements for all platforms:

- C/C++ compiler with support for C++17
- CMake 3.8 or newer
- Doxygen 1.8.0 or newer (to build documentation; optional)

Additional building requirements for Windows platform:
- [NVIDIA CUDA Toolkit](https://developer.nvidia.com/cuda-toolkit) (`nvcc` needs to be in `PATH`)

libavstream can be built either with standalone CMake or using VS2017 CMake project type.

# Usage examples

### Encoding

This example encodes HEVC video stream using D3D11 texture as input surface and sends it to an UDP endpoint.
Error checking has been omitted for brevity.

```cpp
#include <libavstream/libavstream.hpp>

// Valid D3D11 hardware device.
ID3D11Device* gDevice;
// R8G8B8A8_UNORM or B8G8R8A8_UNORM texture.
ID3D11Texture2D* gTexture;

int main()
{
	avs::Context context;
	
	// Use D3D11 texture as input surface.
	avs::Surface surface;
	surface.configure(new avs::SurfaceDX11{gTexture});
	
	// Encode 1920x1080 HEVC stream.
	avs::Encoder encoder;
	avs::EncoderParams params = {};
	params.codec = avs::VideoCodec::HEVC;
	encoder.configure(avs::DeviceHandle{avs::DeviceType::DirectX, gDevice}, 1920, 1080, params);

	// Parse encoder output into individual NAL units.
	avs::Packetizer packetizer(avs::StreamParser::AVC_AnnexB);
	packetizer.configure(1);

	// Bind to local endpoint at port 1666 and send to remote endpoint at 127.0.0.1:1667.
	avs::NetworkSink sink;
	sink.configure(1, 1666, "127.0.0.1", 1667);

	// Build our pipeline.
	avs::Pipeline pipeline;
	pipeline.link({ &surface, &encoder, &packetizer, &sink });
	
	while(eventLoopRunning())
	{
		// Update.
		// Render.
		pipeline.process();
		// Present.
	}
}
```

### Decoding

This example receives HEVC video stream from an UDP endpoint and decodes it to a D3D11 texture. Error checking has been omitted for brevity.

```cpp
#include <libavstream/libavstream.hpp>

// Valid D3D11 hardware device.
ID3D11Device* gDevice;
// R8G8B8A8_UNORM or B8G8R8A8_UNORM texture.
ID3D11Texture2D* gTexture;

int main()
{
	avs::Context context;
	
	// Bind to local endpoint at port 1667 and receive from remote endpoint at 127.0.0.1:1666
	avs::NetworkSource source;
	avs::NetworkSourceParams sourceParams = {};
	sourceParams.maxJitterBufferLength = 50; // Cap jitter buffer at 50ms.
	source.configure(1, 1667, "127.0.0.1", 1666, sourceParams);
	
	// Decode 1920x1080 HEVC stream.
	avs::Decoder decoder;
	avs::DecoderParams decoderParams = {};
	decoderParams.codec = avs::VideoCodec::HEVC;
	decoder.configure(avs::DeviceHandle{avs::DeviceType::DirectX, gDevice}, 1920, 1080, decoderParams);
	
	// Use D3D11 texture as output surface.
	avs::Surface surface;
	surface.configure(new avs::SurfaceDX11{gTexture});

	// Build our pipeline.
	avs::Pipeline pipeline;
	pipeline.link({ &source, &decoder, &surface });

	while(eventLoopRunning())
	{
		// Update.
		pipeline.process();
		// Render.
		// Present.
	}
}
```
