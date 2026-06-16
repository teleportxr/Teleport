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
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <ktx.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace teleport::server;
namespace fs = std::filesystem;

namespace
{
	// Same cube map face convention as CubemapKTX2Transformer (GPU/Vulkan/KTX).
	void FaceUVToDir(int face, float u, float v, float d[3])
	{
		switch (face)
		{
		case 0: d[0] =  1; d[1] = -v; d[2] = -u; break;	// +X
		case 1: d[0] = -1; d[1] = -v; d[2] =  u; break;	// -X
		case 2: d[0] =  u; d[1] =  1; d[2] =  v; break;	// +Y
		case 3: d[0] =  u; d[1] = -1; d[2] = -v; break;	// -Y
		case 4: d[0] =  u; d[1] = -v; d[2] =  1; break;	// +Z
		default:d[0] = -u; d[1] = -v; d[2] = -1; break;	// -Z
		}
	}
	void DirToFaceUV(const float d[3], int &face, float &u, float &v)
	{
		float ax = std::fabs(d[0]), ay = std::fabs(d[1]), az = std::fabs(d[2]);
		if (ax >= ay && ax >= az)
		{
			float ma = ax;
			if (d[0] > 0) { face = 0; u = -d[2] / ma; v = -d[1] / ma; }
			else          { face = 1; u =  d[2] / ma; v = -d[1] / ma; }
		}
		else if (ay >= ax && ay >= az)
		{
			float ma = ay;
			if (d[1] > 0) { face = 2; u =  d[0] / ma; v =  d[2] / ma; }
			else          { face = 3; u =  d[0] / ma; v = -d[2] / ma; }
		}
		else
		{
			float ma = az;
			if (d[2] > 0) { face = 4; u =  d[0] / ma; v = -d[1] / ma; }
			else          { face = 5; u = -d[0] / ma; v = -d[1] / ma; }
		}
	}
	// Engineering->OpenGL direction: (x, z, -y).
	void DirEngToGl(const float in[3], float out[3]) { out[0] = in[0]; out[1] = in[2]; out[2] = -in[1]; }

	float HalfToFloat(uint16_t h)
	{
		uint32_t sign = (h & 0x8000u) << 16;
		uint32_t exp = (h >> 10) & 0x1F;
		uint32_t mant = h & 0x3FF;
		uint32_t bits;
		if (exp == 0)
		{
			if (mant == 0) bits = sign;					// +/- zero
			else											// subnormal -> normalise
			{
				exp = 127 - 15 + 1;
				while ((mant & 0x400) == 0) { mant <<= 1; exp--; }
				mant &= 0x3FF;
				bits = sign | (exp << 23) | (mant << 13);
			}
		}
		else if (exp == 0x1F) bits = sign | 0x7F800000 | (mant << 13);	// inf/nan
		else bits = sign | ((exp - 15 + 127) << 23) | (mant << 13);
		float f; std::memcpy(&f, &bits, 4); return f;
	}

	// Bilinear sample a face image (RGBA16F) at face coords u,v in [-1,1]; returns linear RGB.
	void SampleFace(const std::vector<uint8_t> &face, uint32_t N, float u, float v, float rgb[3])
	{
		float fx = (u + 1.0f) * 0.5f * (float)N - 0.5f;
		float fy = (v + 1.0f) * 0.5f * (float)N - 0.5f;
		int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
		float tx = fx - x0, ty = fy - y0;
		auto clampi = [&](int i) { return i < 0 ? 0 : (i >= (int)N ? (int)N - 1 : i); };
		const uint16_t *p = reinterpret_cast<const uint16_t *>(face.data());
		for (int c = 0; c < 3; ++c)
		{
			auto tex = [&](int x, int y) { return HalfToFloat(p[((size_t)clampi(y) * N + clampi(x)) * 4 + c]); };
			float a = tex(x0, y0) * (1 - tx) + tex(x0 + 1, y0) * tx;
			float b = tex(x0, y0 + 1) * (1 - tx) + tex(x0 + 1, y0 + 1) * tx;
			rgb[c] = a * (1 - ty) + b * ty;
		}
	}

	// Render an equirectangular PNG of a cubemap. The cubemap is interpreted as being in axes
	// `srcAxesIsGl ? OpenGL : Engineering`; sampling is parameterised in a common Engineering world
	// frame so two correctly-related cubemaps (eng original and its gl conversion) produce identical
	// images. forward(lon=0)=+Y, right=+X, up=+Z.
	bool DumpEquirect(const ktxTexture2 *src, bool srcAxesIsGl, const std::string &outPng, int W = 1024)
	{
		uint32_t N = 0;
		std::vector<std::vector<uint8_t>> faces;
		if (!CubemapKTX2Transformer::DecodeFacesRGBA16F(src, 0, N, faces))
			return false;
		int H = W / 2;
		std::vector<uint8_t> img((size_t)W * H * 3);
		for (int py = 0; py < H; ++py)
		{
			float lat = (float)M_PI * 0.5f - ((float)py + 0.5f) / (float)H * (float)M_PI;
			for (int px = 0; px < W; ++px)
			{
				float lon = ((float)px + 0.5f) / (float)W * 2.0f * (float)M_PI - (float)M_PI;
				float world[3] = { std::sin(lon) * std::cos(lat), std::cos(lon) * std::cos(lat), std::sin(lat) };
				float d[3];
				if (srcAxesIsGl) DirEngToGl(world, d); else { d[0] = world[0]; d[1] = world[1]; d[2] = world[2]; }
				int f; float u, v;
				DirToFaceUV(d, f, u, v);
				float rgb[3];
				SampleFace(faces[f], N, u, v, rgb);
				uint8_t *o = &img[((size_t)py * W + px) * 3];
				for (int c = 0; c < 3; ++c)
				{
					float t = rgb[c] / (1.0f + rgb[c]);			// Reinhard tonemap
					float s = std::pow(std::max(0.0f, t), 1.0f / 2.2f);	// gamma
					int iv = (int)(s * 255.0f + 0.5f);
					o[c] = (uint8_t)(iv < 0 ? 0 : (iv > 255 ? 255 : iv));
				}
			}
		}
		return stbi_write_png(outPng.c_str(), W, H, 3, img.data(), W * 3) != 0;
	}
}

struct Options
{
	std::string inputFile;
	std::string outputFile;
	bool convertEngToGL = true;
	bool convertGLToEng = false;
	bool useCustomMapping = false;
	CubemapKTX2Transformer::Face customMapping[6] = {};
	bool verbose = false;
	// Verification: render an equirectangular PNG of the input cubemap instead of converting.
	std::string equirectFile;
	bool inputIsGlAxes = false;	// interpret the input file as OpenGL axes (default: Engineering)
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
		else if (arg == "-eq" || arg == "--equirect")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Error: --equirect requires an output .png path\n";
				return false;
			}
			opts.equirectFile = argv[++i];
		}
		else if (arg == "--axes")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Error: --axes requires eng|gl\n";
				return false;
			}
			std::string a = argv[++i];
			opts.inputIsGlAxes = (a == "gl" || a == "ogl");
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

	// Equirect mode only reads the input cubemap; no output .ktx2 is required.
	if (opts.inputFile.empty() || (opts.outputFile.empty() && opts.equirectFile.empty()))
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

	// Verification mode: render an equirectangular projection and exit.
	if (!opts.equirectFile.empty())
	{
		std::cout << "Rendering equirectangular projection (" << (opts.inputIsGlAxes ? "gl" : "eng")
				  << " axes) to: " << opts.equirectFile << "\n";
		bool ok = DumpEquirect(source, opts.inputIsGlAxes, opts.equirectFile);
		CubemapKTX2Transformer::DestroyTexture(source);
		std::cout << (ok ? "✓ Wrote " : "Error: failed to write ") << opts.equirectFile << "\n";
		return ok ? 0 : 1;
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
