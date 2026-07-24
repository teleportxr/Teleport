/// GltfConverter - Command-line utility for converting between glTF's binary (.glb)
/// and text/JSON (.gltf) containers. Also handles .vrm and .vrma, which are glTF-binary
/// containers with extra top-level extensions (VRM / VRMC_vrm / VRMC_vrm_animation) - they
/// are converted exactly like .glb, with all extension JSON and buffer/image bytes preserved
/// byte-for-byte.
/// Usage: GltfConverter [options] <input> [output]
///
/// Options:
///   -h, --help                Show this help message
///   -o, --output <path>       Output file (default: same stem, opposite container extension)
///   -p, --pretty              Pretty-print JSON output (text output only)
///   -x, --external-buffers    Write buffers/images as external files instead of embedding them (text output only)
///   -v, --verbose             Verbose output

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// This is the only translation unit in GltfConverter, so it owns the single
// TINYGLTF_IMPLEMENTATION for this executable. The `tinygltf` namespace is renamed to
// avoid clashing with the separately-compiled `tinygltf::` symbols baked into the `draco`
// library (thirdparty/draco/src/draco/io/tiny_gltf_utils.cc, built with
// DRACO_TRANSCODER_SUPPORTED) if this tool is ever linked alongside draco.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define tinygltf teleport_tinygltf
#include "tiny_gltf.h"
#undef tinygltf

namespace fs = std::filesystem;

namespace
{
	enum class Container
	{
		Text,	// .gltf - JSON, with buffers/images embedded as base64 or referenced externally
		Binary	// .glb, .vrm, .vrma - single binary container
	};

	bool DetectContainer(const std::string &path, Container &container)
	{
		std::string ext = fs::path(path).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		if (ext == ".gltf")
		{
			container = Container::Text;
			return true;
		}
		if (ext == ".glb" || ext == ".vrm" || ext == ".vrma")
		{
			container = Container::Binary;
			return true;
		}
		return false;
	}

	std::string DefaultOutputPath(const std::string &inputFile, Container inputContainer)
	{
		fs::path p(inputFile);
		p.replace_extension(inputContainer == Container::Text ? ".glb" : ".gltf");
		return p.string();
	}

	// GLB/VRM/VRMA-embedded images (image.bufferView != -1) already round-trip byte-for-byte:
	// tinygltf's writer only calls WriteImageData when it has a filename to write to, and an
	// image with a bufferView never gets one - it keeps referencing the original buffer bytes
	// untouched. So the only thing this loader must do for that case is not fail; it must not
	// decode pixels (that would be lossy and pointless for a pure container-format conversion).
	//
	// The only case that needs real handling here is a .gltf image encoded as an inline base64
	// data URI (no bufferView, no uri - tinygltf decodes it and would otherwise hand us the raw
	// bytes only to discard them if we don't keep them). Stash those raw bytes verbatim so
	// WriteOpaqueImageData can re-emit them unchanged instead of silently dropping the image.
	bool LoadOpaqueImageData(teleport_tinygltf::Image *image, const int, std::string *, std::string *,
		int, int, const unsigned char *bytes, int size, void *)
	{
		if (image->bufferView != -1)
			return true;

		if (bytes && size > 0)
			image->image.assign(bytes, bytes + size);
		return true;
	}

	// Only reached for the inline-base64-image case above (image.image non-empty with no
	// bufferView) - re-emit the untouched bytes as a data URI rather than re-encoding pixels.
	bool WriteOpaqueImageData(const std::string *, const std::string *, const teleport_tinygltf::Image *image,
		bool, const teleport_tinygltf::URICallbacks *, std::string *out_uri, void *)
	{
		const std::string mimeType = image->mimeType.empty() ? "application/octet-stream" : image->mimeType;
		*out_uri = "data:" + mimeType + ";base64," +
			teleport_tinygltf::base64_encode(image->image.data(), static_cast<unsigned int>(image->image.size()));
		return true;
	}

	bool HasExtension(const teleport_tinygltf::Model &model, const std::string &name)
	{
		return std::find(model.extensionsUsed.begin(), model.extensionsUsed.end(), name) != model.extensionsUsed.end();
	}

	constexpr uint32_t GLB_MAGIC	  = 0x46546C67;	// "glTF"
	constexpr uint32_t GLB_CHUNK_JSON = 0x4E4F534A;	// "JSON"
	constexpr uint32_t GLB_CHUNK_BIN  = 0x004E4942;	// "BIN\0"

	// tinygltf's generic extension `Value` model silently drops empty JSON arrays/objects when it
	// re-serializes `extensions` (confirmed empirically: VRM's blendShapeMaster/materialProperties
	// commonly contain empty `binds`/`materialValues`/etc for unused presets, and these keys vanish
	// on any tinygltf load+write cycle, regardless of container format - it is not specific to this
	// tool). model.extensions_json_string, captured via SetStoreOriginalJSONForExtrasAndExtensions,
	// is built directly from the original parsed JSON document rather than from the lossy `Value`
	// tree (see tiny_gltf.h's `model->extensions_json_string = JsonToString(v["extensions"]);`), so
	// it is a byte-faithful copy of the root-level extensions object. Splice it back into the
	// already-written output so VRM/VRMC_vrm/VRMC_vrm_animation payloads survive exactly as authored.
	bool PatchRootExtensions(const std::string &outputFile, bool writeBinary, bool pretty, const std::string &originalExtensionsJson)
	{
		if (originalExtensionsJson.empty())
			return true;

		json originalExtensions = json::parse(originalExtensionsJson);

		if (!writeBinary)
		{
			std::ifstream in(outputFile, std::ios::binary);
			if (!in)
				return false;
			json doc = json::parse(in);
			in.close();
			doc["extensions"] = originalExtensions;

			std::ofstream out(outputFile, std::ios::binary | std::ios::trunc);
			if (!out)
				return false;
			out << doc.dump(pretty ? 2 : -1);
			return (bool)out;
		}

		std::ifstream in(outputFile, std::ios::binary);
		if (!in)
			return false;
		std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		in.close();
		if (data.size() < 20)
			return false;

		uint32_t magic = 0, version = 0, totalLength = 0;
		std::memcpy(&magic, &data[0], 4);
		std::memcpy(&version, &data[4], 4);
		std::memcpy(&totalLength, &data[8], 4);
		if (magic != GLB_MAGIC)
			return false;

		std::string				   jsonText;
		std::vector<unsigned char> binChunk;
		bool						hasBin = false;
		size_t						offset = 12;
		while (offset + 8 <= data.size())
		{
			uint32_t chunkLength = 0, chunkType = 0;
			std::memcpy(&chunkLength, &data[offset], 4);
			std::memcpy(&chunkType, &data[offset + 4], 4);
			size_t chunkDataStart = offset + 8;
			if (chunkDataStart + chunkLength > data.size())
				return false;
			if (chunkType == GLB_CHUNK_JSON)
				jsonText.assign(reinterpret_cast<const char *>(&data[chunkDataStart]), chunkLength);
			else if (chunkType == GLB_CHUNK_BIN)
			{
				binChunk.assign(data.begin() + static_cast<long>(chunkDataStart), data.begin() + static_cast<long>(chunkDataStart + chunkLength));
				hasBin = true;
			}
			offset = chunkDataStart + chunkLength;
		}
		if (jsonText.empty())
			return false;

		json doc = json::parse(jsonText);
		doc["extensions"]			 = originalExtensions;
		std::string newJsonText	 = doc.dump();
		while (newJsonText.size() % 4 != 0)
			newJsonText.push_back(' ');	// GLB chunks must be 4-byte aligned; pad JSON with spaces per spec.

		std::vector<unsigned char> out;
		auto						appendU32 = [&out](uint32_t v)
		{
			const unsigned char *p = reinterpret_cast<const unsigned char *>(&v);
			out.insert(out.end(), p, p + 4);
		};

		const uint32_t jsonChunkLen	  = static_cast<uint32_t>(newJsonText.size());
		const uint32_t newTotalLength = 12 + 8 + jsonChunkLen + (hasBin ? (8 + static_cast<uint32_t>(binChunk.size())) : 0);

		appendU32(magic);
		appendU32(version);
		appendU32(newTotalLength);
		appendU32(jsonChunkLen);
		appendU32(GLB_CHUNK_JSON);
		out.insert(out.end(), newJsonText.begin(), newJsonText.end());
		if (hasBin)
		{
			// tinygltf always writes BIN chunks already zero-padded to a 4-byte boundary, so the
			// captured binChunk (sized from the chunk's declared length) needs no extra padding here.
			appendU32(static_cast<uint32_t>(binChunk.size()));
			appendU32(GLB_CHUNK_BIN);
			out.insert(out.end(), binChunk.begin(), binChunk.end());
		}

		std::ofstream o(outputFile, std::ios::binary | std::ios::trunc);
		if (!o)
			return false;
		o.write(reinterpret_cast<const char *>(out.data()), static_cast<std::streamsize>(out.size()));
		return (bool)o;
	}

	struct Options
	{
		std::string inputFile;
		std::string outputFile;
		bool		pretty			 = false;
		bool		externalBuffers	 = false;
		bool		verbose			 = false;
	};

	void PrintUsage(const char *argv0)
	{
		std::cout << "Usage: " << argv0 << " [options] <input> [output]\n\n"
				  << "Converts between glTF binary (.glb) and glTF text (.gltf) containers.\n"
				  << ".vrm and .vrma are treated as glTF binary - their VRM extension JSON\n"
				  << "and embedded buffers/images round-trip unchanged.\n\n"
				  << "Options:\n"
				  << "  -h, --help                Show this help message\n"
				  << "  -o, --output <path>       Output file (default: same stem, opposite container extension)\n"
				  << "  -p, --pretty              Pretty-print JSON output (text output only)\n"
				  << "  -x, --external-buffers    Write buffers/images as external files instead of embedding them (text output only)\n"
				  << "  -v, --verbose             Verbose output\n\n"
				  << "Examples:\n"
				  << "  " << argv0 << " avatar.vrm avatar.gltf\n"
				  << "  " << argv0 << " avatar.gltf avatar.vrm\n"
				  << "  " << argv0 << " Idle.vrma Idle.gltf --pretty\n";
	}

	bool ParseArguments(int argc, char **argv, Options &opts)
	{
		for (int i = 1; i < argc; i++)
		{
			std::string arg = argv[i];
			if (arg == "-h" || arg == "--help")
			{
				PrintUsage(argv[0]);
				std::exit(0);
			}
			else if (arg == "-o" || arg == "--output")
			{
				if (i + 1 >= argc)
				{
					std::cerr << "Error: " << arg << " requires a path argument\n";
					return false;
				}
				opts.outputFile = argv[++i];
			}
			else if (arg == "-p" || arg == "--pretty")
			{
				opts.pretty = true;
			}
			else if (arg == "-x" || arg == "--external-buffers")
			{
				opts.externalBuffers = true;
			}
			else if (arg == "-v" || arg == "--verbose")
			{
				opts.verbose = true;
			}
			else if (!arg.empty() && arg[0] == '-')
			{
				std::cerr << "Error: Unknown option: " << arg << "\n";
				PrintUsage(argv[0]);
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

		if (opts.inputFile.empty())
		{
			std::cerr << "Error: Input file is required\n";
			PrintUsage(argv[0]);
			return false;
		}

		return true;
	}
}

int main(int argc, char **argv)
{
	Options opts;

	if (!ParseArguments(argc, argv, opts))
		return 1;

	if (!fs::exists(opts.inputFile))
	{
		std::cerr << "Error: Input file not found: " << opts.inputFile << "\n";
		return 1;
	}

	Container inputContainer;
	if (!DetectContainer(opts.inputFile, inputContainer))
	{
		std::cerr << "Error: Unrecognized input extension for: " << opts.inputFile
				  << " (expected .gltf, .glb, .vrm or .vrma)\n";
		return 1;
	}

	if (opts.outputFile.empty())
		opts.outputFile = DefaultOutputPath(opts.inputFile, inputContainer);

	Container outputContainer;
	if (!DetectContainer(opts.outputFile, outputContainer))
	{
		std::cerr << "Error: Unrecognized output extension for: " << opts.outputFile
				  << " (expected .gltf, .glb, .vrm or .vrma)\n";
		return 1;
	}

	teleport_tinygltf::Model	 model;
	teleport_tinygltf::TinyGLTF io;
	io.SetStoreOriginalJSONForExtrasAndExtensions(true);
	io.SetImageLoader(&LoadOpaqueImageData, nullptr);
	io.SetImageWriter(&WriteOpaqueImageData, nullptr);

	std::string err, warn;

	if (opts.verbose)
		std::cout << "Loading " << (inputContainer == Container::Text ? "text" : "binary") << " glTF from: " << opts.inputFile << "\n";

	bool loaded = (inputContainer == Container::Text)
					  ? io.LoadASCIIFromFile(&model, &err, &warn, opts.inputFile)
					  : io.LoadBinaryFromFile(&model, &err, &warn, opts.inputFile);

	if (!warn.empty())
		std::cerr << "Warning: " << warn;

	if (!loaded)
	{
		std::cerr << "Error: Failed to load " << opts.inputFile << "\n";
		if (!err.empty())
			std::cerr << "Error: " << err;
		return 1;
	}

	if (opts.verbose)
	{
		std::cout << "  Buffers: " << model.buffers.size() << ", Images: " << model.images.size()
				  << ", Meshes: " << model.meshes.size() << ", Nodes: " << model.nodes.size()
				  << ", Animations: " << model.animations.size() << "\n";
		if (HasExtension(model, "VRM"))
			std::cout << "  Detected VRM 0.x avatar (extension \"VRM\")\n";
		if (HasExtension(model, "VRMC_vrm"))
			std::cout << "  Detected VRM 1.0 avatar (extension \"VRMC_vrm\")\n";
		if (HasExtension(model, "VRMC_vrm_animation"))
			std::cout << "  Detected VRMA animation (extension \"VRMC_vrm_animation\")\n";
	}

	const bool writeBinary	 = (outputContainer == Container::Binary);
	const bool embedBuffers = !opts.externalBuffers;
	const bool embedImages	 = !opts.externalBuffers;

	// tinygltf only embeds buffer[0] into the GLB's binary chunk when its `uri` is empty
	// (tiny_gltf.h: `if (writeBinary && i == 0 && model->buffers[i].uri.empty())`). After a
	// gltf(text)->glb round trip buffer[0].uri still holds the base64 data URI it was loaded
	// with, which would otherwise make binary output re-embed the whole buffer as base64 inside
	// the JSON chunk instead of writing a proper (much smaller) BIN chunk. The bytes are already
	// resident in buffer.data regardless of where they came from, so clearing uri is safe here.
	if (writeBinary && !model.buffers.empty())
		model.buffers[0].uri.clear();

	if (opts.verbose)
		std::cout << "Writing " << (writeBinary ? "binary" : "text") << " glTF to: " << opts.outputFile << "\n";

	if (!io.WriteGltfSceneToFile(&model, opts.outputFile, embedImages, embedBuffers, opts.pretty, writeBinary))
	{
		std::cerr << "Error: Failed to write " << opts.outputFile << "\n";
		return 1;
	}

	if (!PatchRootExtensions(opts.outputFile, writeBinary, opts.pretty, model.extensions_json_string))
	{
		std::cerr << "Error: Failed to restore root-level extensions JSON in " << opts.outputFile << "\n";
		return 1;
	}

	std::cout << "✓ " << opts.inputFile << " → " << opts.outputFile << "\n";
	return 0;
}
