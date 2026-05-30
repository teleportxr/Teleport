/// CubemapConverter - Command-line utility for transforming KTX2 cubemap files
/// Usage: CubemapConverter [options] <input.ktx2> <output.ktx2>
///
/// Options:
///   -h, --help                Show this help message
///   -e2g, --eng-to-gl         Convert Engineering axes to OpenGL (default)
///   -g2e, --gl-to-eng         Convert OpenGL axes to Engineering
///   -v, --verbose             Verbose output
///   -f, --faces <mapping>     Custom face mapping (e.g., "0,1,4,5,2,3")

#include "TeleportServer/CubemapRearrangement.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>

using namespace teleport::server;
namespace fs = std::filesystem;

struct Options
{
	std::string inputFile;
	std::string outputFile;
	bool convertEngToGL = true;
	bool convertGLToEng = false;
	bool useCustomMapping = false;
	CubemapKTX2Transformer::Face customMapping[6] = {};
	bool verbose = false;
};

void PrintUsage(const char* programName)
{
	std::cout << "CubemapConverter - KTX2 Cubemap Transformation Utility\n\n"
		<< "Usage: " << programName << " [options] <input.ktx2> <output.ktx2>\n\n"
		<< "Options:\n"
		<< "  -h, --help              Show this help message\n"
		<< "  -e2g, --eng-to-gl       Convert Engineering → OpenGL (default)\n"
		<< "  -g2e, --gl-to-eng       Convert OpenGL → Engineering\n"
		<< "  -v, --verbose           Verbose output\n"
		<< "  -f, --faces <mapping>   Custom face mapping (comma-separated, 0-5)\n"
		<< "                          Example: 0,1,4,5,2,3 (faces in new order)\n\n"
		<< "Examples:\n"
		<< "  # Engineering to OpenGL (swap Y/Z axes)\n"
		<< "  " << programName << " -e2g scene_eng.ktx2 scene_gl.ktx2\n\n"
		<< "  # OpenGL to Engineering\n"
		<< "  " << programName << " -g2e scene_gl.ktx2 scene_eng.ktx2\n\n"
		<< "  # Custom face rearrangement\n"
		<< "  " << programName << " -f 1,0,3,2,5,4 input.ktx2 output.ktx2\n\n"
		<< "Face indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z\n";
}

bool ParseArguments(int argc, char** argv, Options& opts)
{
	if (argc < 3)
	{
		PrintUsage(argv[0]);
		return false;
	}

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help")
		{
			PrintUsage(argv[0]);
			return false;
		}
		else if (arg == "-e2g" || arg == "--eng-to-gl")
		{
			opts.convertEngToGL = true;
			opts.convertGLToEng = false;
			opts.useCustomMapping = false;
		}
		else if (arg == "-g2e" || arg == "--gl-to-eng")
		{
			opts.convertGLToEng = true;
			opts.convertEngToGL = false;
			opts.useCustomMapping = false;
		}
		else if (arg == "-v" || arg == "--verbose")
		{
			opts.verbose = true;
		}
		else if (arg == "-f" || arg == "--faces")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Error: --faces requires an argument\n";
				return false;
			}
			std::string mapping = argv[++i];
			opts.useCustomMapping = true;

			// Parse comma-separated face indices
			std::vector<int> faces;
			size_t pos = 0;
			while (pos < mapping.length())
			{
				size_t comma = mapping.find(',', pos);
				if (comma == std::string::npos)
					comma = mapping.length();

				int face = std::stoi(mapping.substr(pos, comma - pos));
				if (face < 0 || face > 5)
				{
					std::cerr << "Error: Face index must be 0-5, got " << face << "\n";
					return false;
				}
				faces.push_back(face);
				pos = comma + 1;
			}

			if (faces.size() != 6)
			{
				std::cerr << "Error: Expected 6 face indices, got " << faces.size() << "\n";
				return false;
			}

			for (int j = 0; j < 6; ++j)
				opts.customMapping[j] = static_cast<CubemapKTX2Transformer::Face>(faces[j]);
		}
		else if (arg[0] == '-')
		{
			std::cerr << "Error: Unknown option " << arg << "\n";
			return false;
		}
		else if (opts.inputFile.empty())
		{
			opts.inputFile = arg;
		}
		else if (opts.outputFile.empty())
		{
			opts.outputFile = arg;
		}
		else
		{
			std::cerr << "Error: Too many positional arguments\n";
			return false;
		}
	}

	if (opts.inputFile.empty() || opts.outputFile.empty())
	{
		std::cerr << "Error: Input and output files are required\n";
		PrintUsage(argv[0]);
		return false;
	}

	return true;
}

int main(int argc, char** argv)
{
	Options opts;

	if (!ParseArguments(argc, argv, opts))
		return 1;

	// Validate input file exists
	if (!fs::exists(opts.inputFile))
	{
		std::cerr << "Error: Input file not found: " << opts.inputFile << "\n";
		return 1;
	}

	if (opts.verbose)
	{
		std::cout << "Loading cubemap from: " << opts.inputFile << "\n";
	}

	// Load input KTX2 cubemap
	ktxTexture2* source = CubemapKTX2Transformer::LoadKTX2File(opts.inputFile);
	if (!source)
	{
		std::cerr << "Error: Failed to load KTX2 cubemap\n";
		return 1;
	}

	if (opts.verbose)
	{
		uint32_t w, h, m;
		CubemapKTX2Transformer::GetDimensions(source, w, h, m);
		std::cout << "  Dimensions: " << w << "x" << h << ", Mips: " << m << "\n";
	}

	// Transform
	ktxTexture2* transformed = nullptr;

	if (opts.useCustomMapping)
	{
		if (opts.verbose)
			std::cout << "Applying custom face mapping...\n";
		transformed = CubemapKTX2Transformer::TransformFaceOrder(source, opts.customMapping);
	}
	else if (opts.convertEngToGL)
	{
		if (opts.verbose)
			std::cout << "Converting Engineering → OpenGL (swapping Y/Z axes)...\n";
		transformed = CubemapKTX2Transformer::ConvertEngineeringToOpenGL(source);
	}
	else if (opts.convertGLToEng)
	{
		if (opts.verbose)
			std::cout << "Converting OpenGL → Engineering (swapping Y/Z axes)...\n";
		transformed = CubemapKTX2Transformer::ConvertOpenGLToEngineering(source);
	}

	if (!transformed)
	{
		std::cerr << "Error: Cubemap transformation failed\n";
		CubemapKTX2Transformer::DestroyTexture(source);
		return 1;
	}

	if (opts.verbose)
	{
		std::cout << "Saving transformed cubemap to: " << opts.outputFile << "\n";
	}

	// Save output
	if (!CubemapKTX2Transformer::SaveKTX2File(transformed, opts.outputFile))
	{
		std::cerr << "Error: Failed to save KTX2 cubemap\n";
		CubemapKTX2Transformer::DestroyTexture(source);
		CubemapKTX2Transformer::DestroyTexture(transformed);
		return 1;
	}

	// Cleanup
	CubemapKTX2Transformer::DestroyTexture(source);
	CubemapKTX2Transformer::DestroyTexture(transformed);

	if (opts.verbose)
	{
		std::cout << "✓ Conversion complete\n";
	}
	else
	{
		std::cout << "✓ " << opts.inputFile << " → " << opts.outputFile << "\n";
	}

	return 0;
}
