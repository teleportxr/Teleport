// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <testapi.hpp>
#include <librenderer.hpp>
#include <libavstream/libavstream.hpp>

#include <cxxopts.hpp>
#include <GLFW/glfw3.h>

#include <imgui.h>

namespace avs::test {

class ClientTest final : public Test
{
	static constexpr size_t   NumStreams = 2;
	static constexpr uint32_t NominalJitterBufferLength = 0;
	static constexpr uint32_t MaxJitterBufferLength = 50;

	static constexpr avs::SurfaceFormat SurfaceFormats[2] = {
		avs::SurfaceFormat::ARGB,
		avs::SurfaceFormat::ARGB,
	};

	void run() override
	{
		auto renderer = std::make_unique<RendererDX11>();

		GLFWwindow* window = renderer->initialize(opt_width / opt_scale, opt_height / opt_scale);
		glfwSetWindowPos(window, 100, 50);

		std::vector<TextureHandle> textures(NumStreams);

		avs::Context context;

		avs::NetworkSource source;
		avs::Decoder decoder[NumStreams];
		avs::Surface surface[NumStreams];

		avs::NetworkSourceParams sourceParams = {};
		sourceParams.nominalJitterBufferLength = NominalJitterBufferLength;
		sourceParams.maxJitterBufferLength = MaxJitterBufferLength;
		if(!source.configure(NumStreams, opt_localPort, opt_remoteAddress.c_str(), opt_remotePort, sourceParams)) {
			throw std::runtime_error("Failed to configure network source node");
		}
		
		avs::DecoderParams decoderParams = {};
		decoderParams.codec = avs::VideoCodec::HEVC;
		decoderParams.deferDisplay = true;
		
		avs::Pipeline pipeline;
		pipeline.add(&source);

		for(size_t i=0; i<NumStreams; ++i) {
			textures[i] = renderer->createTexture(opt_width, opt_height, SurfaceFormats[i]);
		
			if(!decoder[i].configure(renderer->getDevice(), opt_width, opt_height, decoderParams,i)) {
				throw std::runtime_error("Failed to configure decoder node");
			}
			if(!surface[i].configure(textures[i]->createSurface())) {
				throw std::runtime_error("Failed to configure output surface node");
			}

			pipeline.link({ &decoder[i], &surface[i] });
			Node::link(source, decoder[i]);
		}

		pipeline.startProfiling("client.csv");

		while(!glfwWindowShouldClose(window)) {
			pipeline.process();

			renderer->beginFrame();
			displayStatistics(renderer->getStats(), source.getCounterValues());
			renderer->display(textures);
			renderer->endFrame(true);

			glfwPollEvents();
		}

		pipeline.stopProfiling();
	}

	void displayStatistics(const RendererStats& renderStats, const NetworkSourceCounters& counters) const
	{
		static StatBuffer<float> statFPS(100);
		static StatBuffer<float> statJitterBuffer(100);
		static StatBuffer<float> statJitterPush(100);
		static StatBuffer<float> statJitterPop(100);

		statFPS.push(float(renderStats.lastFPS));
		statJitterBuffer.push(float(counters.jitterBufferLength));
		statJitterPush.push(float(counters.jitterBufferPush));
		statJitterPop.push(float(counters.jitterBufferPop));

		ImGui::Begin("Statistics");
		ImGui::Text("Frame #: %d", renderStats.frameCounter);
		ImGui::PlotLines("FPS", statFPS.data(), statFPS.count(), 0, nullptr, 0.0f, 60.0f);
		ImGui::Text("Data received: %.2f MiB", counters.bytesReceived / (1024.0f*1024.0f));
		ImGui::Text("Network packets received: %d", counters.networkPacketsReceived);
		ImGui::Text("Decoder packets received: %d", counters.decoderPacketsReceived);
		ImGui::Text("Network packets dropped: %d", counters.networkPacketsDropped);
		ImGui::Text("Decoder packets dropped: %d", counters.decoderPacketsDropped);
		ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
		ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
		ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
		ImGui::End();
	}

	bool parseOptions(int argc, char* argv[]) override
	{
		cxxopts::Options options("ClientTest", "Streaming client test");
		options.add_options()
			("w,width", "Frame width", cxxopts::value<int>(opt_width)->default_value("3840"), "px")
			("h,height", "Frame height", cxxopts::value<int>(opt_height)->default_value("1920"), "px")
			("s,scale", "Display scale ratio", cxxopts::value<int>(opt_scale)->default_value("2"), "1/N")
			("l,local", "Local UDP port", cxxopts::value<int>(opt_localPort)->default_value("1667"))
			("p,port", "Remote UDP endpoint port", cxxopts::value<int>(opt_remotePort)->default_value("10600"))
			("r,remote", "Remote UDP endpoint address", cxxopts::value<std::string>(opt_remoteAddress)->default_value("127.0.0.1"))
			("help", "Prints this message");

		auto result = options.parse(argc, argv);
		if(result.count("help") > 0) {
			std::cerr << options.help() << "\n";
			return false;
		}
		if(opt_width <= 0 || opt_height <= 0 || opt_scale <= 0) {
			std::cerr << options.help() << "\n";
			return false;
		}
		if(opt_localPort <= 0 || opt_localPort > 65535 || opt_remotePort <= 0 || opt_remotePort > 65535) {
			std::cerr << options.help() << "\n";
			return false;
		}

		return true;
	}

	int opt_width;
	int opt_height;
	int opt_scale;
	int opt_localPort;
	int opt_remotePort;
	std::string opt_remoteAddress;
};

} // avs::test

IMPLEMENT_TEST(avs::test::ClientTest)
