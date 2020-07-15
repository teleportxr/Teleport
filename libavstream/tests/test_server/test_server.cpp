// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <testapi.hpp>
#include <librenderer.hpp>
#include <libavstream/libavstream.hpp>

#include <cxxopts.hpp>
#include <GLFW/glfw3.h>

#include <imgui.h>

namespace avs::test {

	class ServerTest final : public Test
	{
		static constexpr size_t NumStreams = 2;

		void run() override
		{
			auto renderer = std::make_unique<RendererDX11>();

			GLFWwindow* window = renderer->initialize(opt_width / opt_scale, opt_height / opt_scale);
			ShaderHandle shader = renderer->createShader(opt_shaderFile);

			std::vector<TextureHandle> textures(NumStreams);

			avs::Context context;

			avs::Surface surface[NumStreams];
			avs::Encoder encoder[NumStreams];
			avs::Packetizer packetizer[NumStreams];

			avs::EncoderParams encoderParams = {};
			encoderParams.codec = avs::VideoCodec::HEVC;
			encoderParams.averageBitrate = 30 * 1000000;
			encoderParams.maxBitrate = encoderParams.averageBitrate * 2;
			encoderParams.deferOutput = true;

			for (size_t i = 0; i < NumStreams; ++i) {
				textures[i] = renderer->createTexture(opt_width, opt_height, SurfaceFormat::ARGB);

				if (!surface[i].configure(textures[i]->createSurface())) {
					throw std::runtime_error("Failed to configure input surface node");
				}
				if (!encoder[i].configure(renderer->getDevice(), opt_width, opt_height, encoderParams)) {
					throw std::runtime_error("Failed to configure encoder node");
				}
				if (!packetizer[i].configure(avs::StreamParserInterface::Create(avs::StreamParserType::AVC_AnnexB), 1, i)) {
					throw std::runtime_error("Failed to configure packetizer node");
				}
			}

			avs::NetworkSink sink;
			avs::NetworkSinkParams sinkParams = {};
			sinkParams.socketBufferSize = 1024 * 1024 * 2;
			if (!sink.configure(NumStreams, uint16_t(opt_localPort), opt_remoteAddress.c_str(), uint16_t(opt_remotePort), sinkParams)) {
				throw std::runtime_error("Failed to configure network sink node");
			}

			avs::Pipeline pipeline;
			for (size_t i = 0; i < NumStreams; ++i) {
				pipeline.link({ &surface[i], &encoder[i], &packetizer[i] });
				Node::link(packetizer[i], sink);
			}
			pipeline.add(&sink);

			pipeline.startProfiling("server.csv");

			while (!glfwWindowShouldClose(window)) {
				for (size_t i = 0; i < NumStreams; ++i) {
					renderer->render(shader, textures[i], i, (float)glfwGetTime());
				}
				pipeline.process();

				renderer->beginFrame();
				displayStatistics(renderer->getStats(), sink.getCounterValues());
				renderer->display(textures);
				renderer->endFrame(true);

				glfwPollEvents();
			}
			pipeline.stopProfiling();
		}

		void displayStatistics(const RendererStats& renderStats, const NetworkSinkCounters& counters) const
		{
			static StatBuffer<float> statFPS(100);
			statFPS.push(float(renderStats.lastFPS));

			ImGui::Begin("Statistics");
			ImGui::Text("Frame #: %d", renderStats.frameCounter);
			ImGui::PlotLines("FPS", statFPS.data(), statFPS.count(), 0, nullptr, 0.0f, 60.0f);
			ImGui::Text("Data sent: %.2f MiB", counters.bytesSent / (1024.0f * 1024.0f));
			ImGui::Text("Network packets sent: %d", counters.networkPacketsSent);
			ImGui::Text("Decoder packets queued: %d", counters.decoderPacketsQueued);
			ImGui::End();
		}

		bool parseOptions(int argc, char* argv[]) override
		{
			cxxopts::Options options("ServerTest", "Streaming server test");
			options.add_options()
				("w,width", "Frame width", cxxopts::value<int>(opt_width)->default_value("3840"), "px")
				("h,height", "Frame height", cxxopts::value<int>(opt_height)->default_value("1920"), "px")
				("s,scale", "Display scale ratio", cxxopts::value<int>(opt_scale)->default_value("2"), "1/N")
				("l,local", "Local UDP port", cxxopts::value<int>(opt_localPort)->default_value("1666"))
				("p,port", "Remote UDP endpoint port", cxxopts::value<int>(opt_remotePort)->default_value("1667"))
				("r,remote", "Remote UDP endpoint address", cxxopts::value<std::string>(opt_remoteAddress)->default_value("127.0.0.1"))
				("shader", "", cxxopts::value<std::vector<std::string>>())
				("help", "Prints this message");
			options.positional_help("<shader file>");
			options.parse_positional("shader");

			auto result = options.parse(argc, argv);
			if (result.count("help") > 0 || result.count("shader") == 0) {
				std::cerr << options.help() << "\n";
				return false;
			}
			if (opt_width <= 0 || opt_height <= 0 || opt_scale <= 0) {
				std::cerr << options.help() << "\n";
				return false;
			}
			if (opt_localPort <= 0 || opt_localPort > 65535 || opt_remotePort <= 0 || opt_remotePort > 65535) {
				std::cerr << options.help() << "\n";
				return false;
			}

			opt_shaderFile = result["shader"].as<std::vector<std::string>>()[0];
			return true;
		}

		int opt_width;
		int opt_height;
		int opt_scale;
		int opt_localPort;
		int opt_remotePort;
		std::string opt_remoteAddress;
		std::string opt_shaderFile;
	};

} // avs::test

IMPLEMENT_TEST(avs::test::ServerTest)
