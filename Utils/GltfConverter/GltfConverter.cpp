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
///   -s, --split-objects <dir> Export each root object of the input's scene as its own .glb in <dir>,
///                             with textures written alongside as external files
///   -p, --pretty              Pretty-print JSON output (text output only)
///   -x, --external-buffers    Write buffers/images as external files instead of embedding them (text output only)
///   -v, --verbose             Verbose output

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <set>
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

	// Reads back a written .gltf, hands the parsed document to `mutate`, and rewrites it.
	bool PatchTextGltf(const std::string &outputFile, bool pretty, const std::function<void(json &)> &mutate)
	{
		std::ifstream in(outputFile, std::ios::binary);
		if (!in)
			return false;
		json doc = json::parse(in);
		in.close();

		mutate(doc);

		std::ofstream out(outputFile, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;
		out << doc.dump(pretty ? 2 : -1);
		return (bool)out;
	}

	// The same for a written .glb: its JSON chunk is reparsed, mutated and rewritten, with the BIN
	// chunk carried across untouched.
	bool PatchBinaryGltf(const std::string &outputFile, const std::function<void(json &)> &mutate)
	{
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
		mutate(doc);
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

		const json originalExtensions = json::parse(originalExtensionsJson);
		const auto splice			  = [&originalExtensions](json &doc) { doc["extensions"] = originalExtensions; };

		return writeBinary ? PatchBinaryGltf(outputFile, splice) : PatchTextGltf(outputFile, pretty, splice);
	}

	// ===================================== Split mode ======================================
	//
	// Exports each root object of the input's scene as its own .glb, at its own origin, with
	// every texture written next to the outputs as an external file.
	//
	// The arrays that carry binary weight - nodes, meshes, skins, accessors, bufferViews, the
	// buffer itself and animations - are subset per object and reindexed. Materials, textures,
	// images, samplers, cameras and lights are copied whole, at their original indices. That
	// asymmetry is deliberate: index references into those arrays also live inside extension
	// JSON that tinygltf keeps as opaque `Value` data (KHR_materials_* texture infos,
	// KHR_texture_basisu, KHR_texture_transform, KHR_lights_punctual, ...), and reindexing
	// around them would silently point materials at the wrong textures. Keeping them costs a
	// little JSON per object and no binary payload at all, because the images are external.

	// Index maps are per-array vectors of new index, with -1 meaning "not in this object".
	int Remap(const std::vector<int> &map, int index)
	{
		if (index < 0 || index >= (int)map.size())
			return -1;
		return map[(size_t)index];
	}

	std::string SanitiseFilename(const std::string &name)
	{
		std::string out;
		out.reserve(name.size());
		for (char c : name)
		{
			const unsigned char u = (unsigned char)c;
			out.push_back((std::isalnum(u) || c == '_' || c == '-' || c == '.') ? c : '_');
		}
		// Leading/trailing dots would give us ".", ".." or a hidden file.
		while (!out.empty() && out.front() == '.')
			out.erase(out.begin());
		while (!out.empty() && out.back() == '.')
			out.pop_back();
		return out;
	}

	std::string UniqueFilename(std::set<std::string> &used, const std::string &stem, const std::string &extension)
	{
		std::string candidate = stem + extension;
		for (int n = 2; !used.insert(candidate).second; n++)
			candidate = stem + "_" + std::to_string(n) + extension;
		return candidate;
	}

	// Images are never decoded, so the file extension comes from the declared mime type, or
	// failing that from the container's magic bytes.
	std::string ImageFileExtension(const std::string &mimeType, const std::vector<unsigned char> &bytes)
	{
		if (mimeType == "image/png")
			return ".png";
		if (mimeType == "image/jpeg")
			return ".jpg";
		if (mimeType == "image/bmp")
			return ".bmp";
		if (mimeType == "image/gif")
			return ".gif";
		if (mimeType == "image/webp")
			return ".webp";
		if (mimeType == "image/ktx2")
			return ".ktx2";

		auto matches = [&bytes](std::initializer_list<unsigned char> magic, size_t offset)
		{
			if (bytes.size() < offset + magic.size())
				return false;
			size_t i = offset;
			for (unsigned char m : magic)
				if (bytes[i++] != m)
					return false;
			return true;
		};

		if (matches({0x89, 'P', 'N', 'G'}, 0))
			return ".png";
		if (matches({0xFF, 0xD8, 0xFF}, 0))
			return ".jpg";
		if (matches({0xAB, 'K', 'T', 'X', ' ', '2', '0'}, 0))
			return ".ktx2";
		if (matches({'R', 'I', 'F', 'F'}, 0) && matches({'W', 'E', 'B', 'P'}, 8))
			return ".webp";
		if (matches({'B', 'M'}, 0))
			return ".bmp";
		if (matches({'G', 'I', 'F', '8'}, 0))
			return ".gif";
		if (matches({'s', 'B'}, 0))
			return ".basis";
		return ".bin";
	}

	bool WriteBytesToFile(const fs::path &path, const std::vector<unsigned char> &bytes)
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;
		if (!bytes.empty())
			out.write(reinterpret_cast<const char *>(bytes.data()), (std::streamsize)bytes.size());
		return (bool)out;
	}

	// Turns every embedded image into a file on disk referenced by uri, so the objects split out
	// of the model can share one set of texture files. Runs once on the loaded model, before any
	// object is built, so each file is written exactly once. Images that already reference an
	// external file keep their uri untouched; the file itself is copied next to the outputs when
	// we are writing somewhere other than the input's own directory, so the uri still resolves.
	// As everywhere else in this tool, no image is ever decoded - the compressed bytes move
	// verbatim.
	bool ExternaliseImages(teleport_tinygltf::Model &model, const fs::path &outDir, const fs::path &inputDir,
		const std::string &stem, bool verbose)
	{
		std::set<std::string> usedNames;
		for (size_t i = 0; i < model.images.size(); i++)
		{
			teleport_tinygltf::Image &image = model.images[i];

			if (!image.uri.empty())
			{
				std::string decoded = image.uri;
				teleport_tinygltf::URIDecode(image.uri, &decoded, nullptr);
				const fs::path source = inputDir / fs::path(decoded);
				const fs::path dest	  = outDir / fs::path(decoded);
				std::error_code ec;
				if (!fs::exists(source, ec))
				{
					std::cerr << "Warning: image " << i << " references \"" << image.uri
							  << "\", which was not found next to the input - the uri is kept as-is\n";
				}
				else if (fs::weakly_canonical(source, ec) != fs::weakly_canonical(dest, ec))
				{
					if (dest.has_parent_path())
						fs::create_directories(dest.parent_path(), ec);
					fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
					if (ec)
					{
						std::cerr << "Error: Failed to copy texture " << source.string() << " to " << dest.string()
								  << ": " << ec.message() << "\n";
						return false;
					}
					if (verbose)
						std::cout << "  Copied texture: " << dest.string() << "\n";
				}
				usedNames.insert(fs::path(decoded).filename().string());
				continue;
			}

			std::vector<unsigned char> bytes;
			if (image.bufferView >= 0)
			{
				if (image.bufferView >= (int)model.bufferViews.size())
				{
					std::cerr << "Error: image " << i << " references bufferView " << image.bufferView << ", which does not exist\n";
					return false;
				}
				const teleport_tinygltf::BufferView &view = model.bufferViews[(size_t)image.bufferView];
				if (view.buffer < 0 || view.buffer >= (int)model.buffers.size())
				{
					std::cerr << "Error: bufferView " << image.bufferView << " references buffer " << view.buffer << ", which does not exist\n";
					return false;
				}
				const std::vector<unsigned char> &data = model.buffers[(size_t)view.buffer].data;
				if (view.byteOffset + view.byteLength > data.size())
				{
					std::cerr << "Error: bufferView " << image.bufferView << " runs past the end of its buffer\n";
					return false;
				}
				bytes.assign(data.begin() + (std::ptrdiff_t)view.byteOffset,
					data.begin() + (std::ptrdiff_t)(view.byteOffset + view.byteLength));
			}
			else if (!image.image.empty())
			{
				// Inline base64 data uri, whose raw bytes LoadOpaqueImageData stashed for us.
				bytes = image.image;
			}
			else
			{
				std::cerr << "Warning: image " << i << " has no uri, bufferView or data - skipped\n";
				continue;
			}

			std::string label = SanitiseFilename(image.name);
			if (label.empty())
				label = std::to_string(i);
			const std::string filename = UniqueFilename(usedNames, stem + "_" + label, ImageFileExtension(image.mimeType, bytes));
			if (!WriteBytesToFile(outDir / filename, bytes))
			{
				std::cerr << "Error: Failed to write texture " << (outDir / filename).string() << "\n";
				return false;
			}

			image.uri		 = filename;
			image.bufferView = -1;
			// Emptying `image` is what makes tinygltf emit our uri verbatim: UpdateImageObject
			// only invokes the image writer when image data is present, and WriteOpaqueImageData
			// would otherwise turn these bytes straight back into a base64 data uri.
			image.image.clear();
			image.as_is = false;
			if (verbose)
				std::cout << "  Wrote texture: " << (outDir / filename).string() << " (" << bytes.size() << " bytes)\n";
		}
		return true;
	}

	// The scene we split. Falls back to scene 0, and then - for the rare file with no scenes at
	// all - to every node that is nobody's child.
	std::vector<int> SceneRootNodes(const teleport_tinygltf::Model &model)
	{
		int sceneIndex = model.defaultScene;
		if (sceneIndex < 0 || sceneIndex >= (int)model.scenes.size())
			sceneIndex = model.scenes.empty() ? -1 : 0;
		if (sceneIndex >= 0)
			return model.scenes[(size_t)sceneIndex].nodes;

		std::vector<bool> isChild(model.nodes.size(), false);
		for (const teleport_tinygltf::Node &node : model.nodes)
			for (int child : node.children)
				if (child >= 0 && child < (int)model.nodes.size())
					isChild[(size_t)child] = true;

		std::vector<int> roots;
		for (size_t i = 0; i < model.nodes.size(); i++)
			if (!isChild[i])
				roots.push_back((int)i);
		return roots;
	}

	std::vector<int> BuildParentMap(const teleport_tinygltf::Model &model)
	{
		std::vector<int> parentOf(model.nodes.size(), -1);
		for (size_t i = 0; i < model.nodes.size(); i++)
			for (int child : model.nodes[i].children)
				if (child >= 0 && child < (int)model.nodes.size())
					parentOf[(size_t)child] = (int)i;
		return parentOf;
	}

	void GatherSubtree(const teleport_tinygltf::Model &model, int node, std::set<int> &into)
	{
		if (node < 0 || node >= (int)model.nodes.size())
			return;
		if (!into.insert(node).second)	// also guards against a malformed cyclic hierarchy
			return;
		for (int child : model.nodes[(size_t)node].children)
			GatherSubtree(model, child, into);
	}

	bool SubtreeHasContent(const teleport_tinygltf::Model &model, const std::set<int> &subtree)
	{
		for (int n : subtree)
		{
			const teleport_tinygltf::Node &node = model.nodes[(size_t)n];
			if (node.mesh >= 0 || node.camera >= 0)
				return true;
			if (node.extensions.find("KHR_lights_punctual") != node.extensions.end())
				return true;
		}
		return false;
	}

	struct NodeClosure
	{
		std::set<int>	 nodes;	// every source node index the exported object needs
		std::vector<int> roots;	// source node indices that become scene roots - the object's own root first
		bool			 foreign = false;	// true if the closure had to reach outside the object's own subtree
	};

	NodeClosure BuildNodeClosure(const teleport_tinygltf::Model &model, int rootNode, const std::vector<int> &parentOf)
	{
		NodeClosure closure;
		GatherSubtree(model, rootNode, closure.nodes);

		// A skin can reference joints that live outside the object's own subtree - a shared
		// armature, say. Pull those in along with their ancestors (or their bind pose moves) and
		// their descendants, until nothing new appears: an object that loses its joints loses its
		// skinning.
		for (bool grew = true; grew;)
		{
			grew						  = false;
			const std::vector<int> current(closure.nodes.begin(), closure.nodes.end());
			for (int n : current)
			{
				const teleport_tinygltf::Node &node = model.nodes[(size_t)n];
				if (node.skin < 0 || node.skin >= (int)model.skins.size())
					continue;
				const teleport_tinygltf::Skin &skin = model.skins[(size_t)node.skin];

				std::vector<int> referenced = skin.joints;
				if (skin.skeleton >= 0)
					referenced.push_back(skin.skeleton);
				for (int joint : referenced)
				{
					if (joint < 0 || joint >= (int)model.nodes.size())
						continue;
					for (int a = joint; a >= 0; a = parentOf[(size_t)a])
						if (closure.nodes.insert(a).second)
							grew = true;
					const size_t before = closure.nodes.size();
					GatherSubtree(model, joint, closure.nodes);
					grew = grew || closure.nodes.size() != before;
				}
			}
		}

		closure.roots.push_back(rootNode);
		for (int n : closure.nodes)
		{
			if (n == rootNode)
				continue;
			const int parent = parentOf[(size_t)n];
			if (parent < 0 || closure.nodes.find(parent) == closure.nodes.end())
			{
				closure.roots.push_back(n);
				closure.foreign = true;
			}
		}
		return closure;
	}

	// Whole-document extensions whose payloads index nodes/meshes of the original scene. They
	// cannot survive a split, so they are dropped rather than left pointing at the wrong nodes.
	const char *const kDocumentScopedExtensions[] = {"VRM", "VRMC_vrm", "VRMC_vrm_animation", "VRMC_springBone", "VRMC_node_constraint"};

	bool IsDocumentScopedExtension(const std::string &name)
	{
		for (const char *n : kDocumentScopedExtensions)
			if (name == n)
				return true;
		return false;
	}

	// Builds a standalone model holding just the object rooted at `rootNode`, with that root
	// placed at the origin. See the block comment above for what is subset and what is copied
	// whole.
	bool BuildObjectModel(const teleport_tinygltf::Model &src, int rootNode, const std::vector<int> &parentOf,
		teleport_tinygltf::Model &dst, NodeClosure &closure)
	{
		closure = BuildNodeClosure(src, rootNode, parentOf);

		// --- nodes: std::set iterates in source order, so the copies stay in a stable order ---
		std::vector<int> nodeMap(src.nodes.size(), -1);
		for (int n : closure.nodes)
		{
			nodeMap[(size_t)n] = (int)dst.nodes.size();
			dst.nodes.push_back(src.nodes[(size_t)n]);
		}

		// --- meshes and skins in use ---
		std::set<int> usedMeshes, usedSkins;
		for (int n : closure.nodes)
		{
			const teleport_tinygltf::Node &node = src.nodes[(size_t)n];
			if (node.mesh >= 0 && node.mesh < (int)src.meshes.size())
				usedMeshes.insert(node.mesh);
			if (node.skin >= 0 && node.skin < (int)src.skins.size())
				usedSkins.insert(node.skin);
		}

		// --- animation channels that still have a target in this object ---
		struct KeptAnimation
		{
			int				 source = -1;
			std::vector<int> channels;
			std::set<int>	 samplers;
		};
		std::vector<KeptAnimation> keptAnimations;
		for (size_t a = 0; a < src.animations.size(); a++)
		{
			const teleport_tinygltf::Animation &anim = src.animations[a];
			KeptAnimation						kept;
			kept.source = (int)a;
			for (size_t c = 0; c < anim.channels.size(); c++)
			{
				const teleport_tinygltf::AnimationChannel &channel = anim.channels[c];
				if (Remap(nodeMap, channel.target_node) < 0)
					continue;
				if (channel.sampler < 0 || channel.sampler >= (int)anim.samplers.size())
					continue;
				kept.channels.push_back((int)c);
				kept.samplers.insert(channel.sampler);
			}
			if (!kept.channels.empty())
				keptAnimations.push_back(std::move(kept));
		}

		// --- accessors reachable from the copied meshes, skins and animation samplers ---
		std::set<int> usedAccessors;
		auto		  useAccessor = [&](int a)
		{
			if (a >= 0 && a < (int)src.accessors.size())
				usedAccessors.insert(a);
		};
		for (int m : usedMeshes)
		{
			for (const teleport_tinygltf::Primitive &prim : src.meshes[(size_t)m].primitives)
			{
				useAccessor(prim.indices);
				for (const auto &attribute : prim.attributes)
					useAccessor(attribute.second);
				for (const auto &target : prim.targets)
					for (const auto &attribute : target)
						useAccessor(attribute.second);
			}
		}
		for (int s : usedSkins)
			useAccessor(src.skins[(size_t)s].inverseBindMatrices);
		for (const KeptAnimation &kept : keptAnimations)
		{
			for (int s : kept.samplers)
			{
				useAccessor(src.animations[(size_t)kept.source].samplers[(size_t)s].input);
				useAccessor(src.animations[(size_t)kept.source].samplers[(size_t)s].output);
			}
		}

		// --- bufferViews reachable from those accessors ---
		std::set<int> usedViews;
		auto		  useView = [&](int v)
		{
			if (v >= 0 && v < (int)src.bufferViews.size())
				usedViews.insert(v);
		};
		for (int a : usedAccessors)
		{
			const teleport_tinygltf::Accessor &accessor = src.accessors[(size_t)a];
			useView(accessor.bufferView);
			if (accessor.sparse.isSparse)
			{
				useView(accessor.sparse.indices.bufferView);
				useView(accessor.sparse.values.bufferView);
			}
		}
		// KHR_draco_mesh_compression keeps its compressed stream in a bufferView of its own.
		for (int m : usedMeshes)
			for (const teleport_tinygltf::Primitive &prim : src.meshes[(size_t)m].primitives)
			{
				const auto it = prim.extensions.find("KHR_draco_mesh_compression");
				if (it != prim.extensions.end() && it->second.Has("bufferView"))
					useView(it->second.Get("bufferView").GetNumberAsInt());
			}
		// Every image should be external by now, but an image we could not externalise must keep
		// its bytes rather than end up dangling.
		for (const teleport_tinygltf::Image &image : src.images)
			useView(image.bufferView);

		// --- one buffer, with an empty uri so WriteGltfSceneToFile emits it as the GLB BIN chunk ---
		std::vector<int>			viewMap(src.bufferViews.size(), -1);
		teleport_tinygltf::Buffer buffer;
		for (int v : usedViews)
		{
			const teleport_tinygltf::BufferView &view = src.bufferViews[(size_t)v];
			if (view.buffer < 0 || view.buffer >= (int)src.buffers.size())
			{
				std::cerr << "Error: bufferView " << v << " references buffer " << view.buffer << ", which does not exist\n";
				return false;
			}
			const std::vector<unsigned char> &data = src.buffers[(size_t)view.buffer].data;
			if (view.byteOffset + view.byteLength > data.size())
			{
				std::cerr << "Error: bufferView " << v << " runs past the end of its buffer\n";
				return false;
			}
			// Accessor offsets are relative to the view, so keeping every view 4-byte aligned
			// keeps every component type aligned too.
			while (buffer.data.size() % 4 != 0)
				buffer.data.push_back(0);

			teleport_tinygltf::BufferView copy = view;
			copy.buffer						   = 0;
			copy.byteOffset					   = buffer.data.size();
			buffer.data.insert(buffer.data.end(), data.begin() + (std::ptrdiff_t)view.byteOffset,
				data.begin() + (std::ptrdiff_t)(view.byteOffset + view.byteLength));

			viewMap[(size_t)v] = (int)dst.bufferViews.size();
			dst.bufferViews.push_back(std::move(copy));
		}
		if (!buffer.data.empty())
			dst.buffers.push_back(std::move(buffer));

		// --- accessors ---
		std::vector<int> accessorMap(src.accessors.size(), -1);
		for (int a : usedAccessors)
		{
			teleport_tinygltf::Accessor accessor = src.accessors[(size_t)a];
			accessor.bufferView					 = Remap(viewMap, accessor.bufferView);
			if (accessor.sparse.isSparse)
			{
				accessor.sparse.indices.bufferView = Remap(viewMap, accessor.sparse.indices.bufferView);
				accessor.sparse.values.bufferView  = Remap(viewMap, accessor.sparse.values.bufferView);
			}
			accessorMap[(size_t)a] = (int)dst.accessors.size();
			dst.accessors.push_back(std::move(accessor));
		}

		// --- meshes ---
		std::vector<int> meshMap(src.meshes.size(), -1);
		for (int m : usedMeshes)
		{
			teleport_tinygltf::Mesh mesh = src.meshes[(size_t)m];
			for (teleport_tinygltf::Primitive &prim : mesh.primitives)
			{
				prim.indices = Remap(accessorMap, prim.indices);
				for (auto &attribute : prim.attributes)
					attribute.second = Remap(accessorMap, attribute.second);
				for (auto &target : prim.targets)
					for (auto &attribute : target)
						attribute.second = Remap(accessorMap, attribute.second);
				// `material` is left alone - materials keep their original indices.
				auto it = prim.extensions.find("KHR_draco_mesh_compression");
				if (it != prim.extensions.end() && it->second.IsObject() && it->second.Has("bufferView"))
				{
					const int remapped							  = Remap(viewMap, it->second.Get("bufferView").GetNumberAsInt());
					it->second.Get<teleport_tinygltf::Value::Object>()["bufferView"] = teleport_tinygltf::Value(remapped);
				}
			}
			meshMap[(size_t)m] = (int)dst.meshes.size();
			dst.meshes.push_back(std::move(mesh));
		}

		// --- skins ---
		std::vector<int> skinMap(src.skins.size(), -1);
		for (int s : usedSkins)
		{
			teleport_tinygltf::Skin skin = src.skins[(size_t)s];
			skin.inverseBindMatrices	 = Remap(accessorMap, skin.inverseBindMatrices);
			if (skin.skeleton >= 0)
				skin.skeleton = Remap(nodeMap, skin.skeleton);
			// Joint order is what JOINTS_0 indexes into, so entries are remapped in place and
			// never dropped, even if the source referenced a node that does not exist.
			for (int &joint : skin.joints)
				joint = Remap(nodeMap, joint);
			skinMap[(size_t)s] = (int)dst.skins.size();
			dst.skins.push_back(std::move(skin));
		}

		// --- node fixups, now that every map exists ---
		for (size_t i = 0; i < dst.nodes.size(); i++)
		{
			teleport_tinygltf::Node &node = dst.nodes[i];
			std::vector<int>		 children;
			for (int child : node.children)
			{
				const int remapped = Remap(nodeMap, child);
				if (remapped >= 0)
					children.push_back(remapped);
			}
			node.children = std::move(children);
			node.mesh	  = Remap(meshMap, node.mesh);
			node.skin	  = Remap(skinMap, node.skin);
			// `camera` is left alone - cameras keep their original indices.
		}

		// --- the object's own root sits at the origin ---
		teleport_tinygltf::Node &root = dst.nodes[(size_t)nodeMap[(size_t)rootNode]];
		root.translation.clear();
		root.rotation.clear();
		root.scale.clear();
		root.matrix.clear();

		// --- scene ---
		teleport_tinygltf::Scene scene;
		scene.name = src.nodes[(size_t)rootNode].name;
		for (int r : closure.roots)
			scene.nodes.push_back(nodeMap[(size_t)r]);
		dst.scenes.push_back(std::move(scene));
		dst.defaultScene = 0;

		// --- animations ---
		for (const KeptAnimation &kept : keptAnimations)
		{
			const teleport_tinygltf::Animation &srcAnim = src.animations[(size_t)kept.source];
			teleport_tinygltf::Animation		anim;
			anim.name		= srcAnim.name;
			anim.extensions = srcAnim.extensions;
			anim.extras		= srcAnim.extras;

			std::vector<int> samplerMap(srcAnim.samplers.size(), -1);
			for (int s : kept.samplers)
			{
				teleport_tinygltf::AnimationSampler sampler = srcAnim.samplers[(size_t)s];
				sampler.input								= Remap(accessorMap, sampler.input);
				sampler.output								= Remap(accessorMap, sampler.output);
				samplerMap[(size_t)s]						= (int)anim.samplers.size();
				anim.samplers.push_back(std::move(sampler));
			}
			for (int c : kept.channels)
			{
				teleport_tinygltf::AnimationChannel channel = srcAnim.channels[(size_t)c];
				channel.sampler								= Remap(samplerMap, channel.sampler);
				channel.target_node							= Remap(nodeMap, channel.target_node);
				anim.channels.push_back(std::move(channel));
			}
			dst.animations.push_back(std::move(anim));
		}

		// --- copied whole, at their original indices ---
		dst.materials = src.materials;
		dst.textures  = src.textures;
		dst.images	  = src.images;
		dst.samplers  = src.samplers;
		dst.cameras	  = src.cameras;
		dst.lights	  = src.lights;
		dst.asset	  = src.asset;
		dst.extras	  = src.extras;
		for (teleport_tinygltf::Image &image : dst.images)
			if (image.bufferView >= 0)
				image.bufferView = Remap(viewMap, image.bufferView);

		for (const std::string &name : src.extensionsUsed)
			if (!IsDocumentScopedExtension(name))
				dst.extensionsUsed.push_back(name);
		for (const std::string &name : src.extensionsRequired)
			if (!IsDocumentScopedExtension(name))
				dst.extensionsRequired.push_back(name);

		return true;
	}

	// tinygltf's animation-channel serializer omits `target.node` when the node index is 0
	// (`if (channel.target_node > 0)` in tiny_gltf.h - every other index field there correctly
	// tests against -1). A channel with no target is inert, and node 0 is the common case here
	// because it is the exported object's own root, so the targets are written back in afterwards.
	bool RestoreAnimationTargets(const std::string &outputFile, const teleport_tinygltf::Model &model)
	{
		bool affected = false;
		for (const teleport_tinygltf::Animation &anim : model.animations)
			for (const teleport_tinygltf::AnimationChannel &channel : anim.channels)
				affected = affected || channel.target_node == 0;
		if (!affected)
			return true;

		return PatchBinaryGltf(outputFile,
			[&model](json &doc)
			{
				if (!doc.contains("animations"))
					return;
				for (size_t a = 0; a < model.animations.size(); a++)
					for (size_t c = 0; c < model.animations[a].channels.size(); c++)
					{
						const int target = model.animations[a].channels[c].target_node;
						if (target >= 0)
							doc["animations"][a]["channels"][c]["target"]["node"] = target;
					}
			});
	}

	// Splits the loaded model into one .glb per root object. Returns a process exit code.
	int RunSplit(teleport_tinygltf::Model &model, teleport_tinygltf::TinyGLTF &io, const std::string &inputFile,
		const std::string &splitDir, bool verbose)
	{
		// Meshopt views carry their own buffer/offset fields, which re-slicing would silently
		// invalidate. Refuse rather than write plausible-looking rubbish.
		auto usesMeshopt = [&model]()
		{
			for (const std::string &name : model.extensionsRequired)
				if (name == "EXT_meshopt_compression")
					return true;
			for (const teleport_tinygltf::BufferView &view : model.bufferViews)
				if (view.extensions.find("EXT_meshopt_compression") != view.extensions.end())
					return true;
			return false;
		};
		if (usesMeshopt())
		{
			std::cerr << "Error: " << inputFile << " uses EXT_meshopt_compression, which cannot be split "
					  << "(its bufferViews carry their own buffer offsets). Decompress it first.\n";
			return 1;
		}

		std::error_code ec;
		fs::create_directories(splitDir, ec);
		if (!fs::is_directory(splitDir, ec))
		{
			std::cerr << "Error: Could not create output directory: " << splitDir << "\n";
			return 1;
		}

		fs::path inputDir = fs::path(inputFile).parent_path();
		if (inputDir.empty())
			inputDir = ".";
		std::string stem = SanitiseFilename(fs::path(inputFile).stem().string());
		if (stem.empty())
			stem = "asset";

		// Work out what we are exporting before writing anything, so a file we are going to reject
		// does not leave a directory of textures behind.
		const std::vector<int> parentOf = BuildParentMap(model);
		std::vector<int>	   roots;
		for (int rootNode : SceneRootNodes(model))
		{
			if (rootNode < 0 || rootNode >= (int)model.nodes.size())
				continue;
			std::set<int> subtree;
			GatherSubtree(model, rootNode, subtree);
			if (SubtreeHasContent(model, subtree))
				roots.push_back(rootNode);
			else if (verbose)
				std::cout << "Skipping root node " << rootNode << " (\"" << model.nodes[(size_t)rootNode].name
						  << "\"): no mesh, camera or light in its subtree\n";
		}
		if (roots.empty())
		{
			std::cerr << "Error: No root object with a mesh, camera or light was found in " << inputFile << "\n";
			return 1;
		}

		if (!ExternaliseImages(model, splitDir, inputDir, stem, verbose))
			return 1;

		bool droppedDocumentExtensions = false;
		for (const std::string &name : model.extensionsUsed)
			droppedDocumentExtensions = droppedDocumentExtensions || IsDocumentScopedExtension(name);
		if (droppedDocumentExtensions)
			std::cerr << "Warning: whole-document extensions (VRM/VRMC_*) index the original scene's nodes "
					  << "and are dropped from the split objects\n";

		// Never overwrite the file we are reading from - an object whose name matches the input's
		// takes a suffix instead. Claiming the name up front is all it takes, as UniqueFilename
		// then treats it as already used.
		std::set<std::string> usedNames;
		const std::string	  inputFilename = fs::path(inputFile).filename().string();
		if (fs::weakly_canonical(fs::path(splitDir), ec) == fs::weakly_canonical(inputDir, ec))
			usedNames.insert(inputFilename);

		int written = 0;
		for (int rootNode : roots)
		{
			teleport_tinygltf::Model object;
			NodeClosure				closure;
			if (!BuildObjectModel(model, rootNode, parentOf, object, closure))
				return 1;

			if (closure.foreign)
				std::cerr << "Warning: object \"" << model.nodes[(size_t)rootNode].name
						  << "\" uses joints from outside its own subtree; those nodes keep their original "
						  << "placement while the object's root is moved to the origin\n";

			std::string label = SanitiseFilename(model.nodes[(size_t)rootNode].name);
			if (label.empty())
				label = stem + "_" + std::to_string(rootNode);
			const std::string outputName = UniqueFilename(usedNames, label, ".glb");
			if (outputName != label + ".glb" && label + ".glb" == inputFilename)
				std::cerr << "Warning: object \"" << model.nodes[(size_t)rootNode].name << "\" would overwrite the input file, "
						  << "so it is written as " << outputName << " instead\n";
			const fs::path outputPath = fs::path(splitDir) / outputName;

			if (!io.WriteGltfSceneToFile(&object, outputPath.string(), /*embedImages*/ false, /*embedBuffers*/ true,
					/*prettyPrint*/ false, /*writeBinary*/ true))
			{
				std::cerr << "Error: Failed to write " << outputPath.string() << "\n";
				return 1;
			}
			if (!RestoreAnimationTargets(outputPath.string(), object))
			{
				std::cerr << "Error: Failed to restore animation targets in " << outputPath.string() << "\n";
				return 1;
			}

			written++;
			std::cout << "✓ " << inputFile << " → " << outputPath.string() << "\n";
			if (verbose)
			{
				std::cout << "  Nodes: " << object.nodes.size() << ", Meshes: " << object.meshes.size()
						  << ", Skins: " << object.skins.size() << ", Accessors: " << object.accessors.size()
						  << ", BufferViews: " << object.bufferViews.size()
						  << ", Animations: " << object.animations.size()
						  << ", Buffer bytes: " << (object.buffers.empty() ? 0 : object.buffers[0].data.size()) << "\n";
			}
		}

		std::cout << "Exported " << written << " object" << (written == 1 ? "" : "s") << " to " << splitDir << "\n";
		return 0;
	}

	struct Options
	{
		std::string inputFile;
		std::string outputFile;
		std::string splitDir;
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
				  << "With --split-objects, exports each root object of the input's scene as its\n"
				  << "own .glb at its own origin, with all textures written as external files.\n\n"
				  << "Options:\n"
				  << "  -h, --help                Show this help message\n"
				  << "  -o, --output <path>       Output file (default: same stem, opposite container extension)\n"
				  << "  -s, --split-objects <dir> Export each root object as its own .glb in <dir>, with external textures\n"
				  << "  -p, --pretty              Pretty-print JSON output (text output only)\n"
				  << "  -x, --external-buffers    Write buffers/images as external files instead of embedding them (text output only)\n"
				  << "  -v, --verbose             Verbose output\n\n"
				  << "Examples:\n"
				  << "  " << argv0 << " avatar.vrm avatar.gltf\n"
				  << "  " << argv0 << " avatar.gltf avatar.vrm\n"
				  << "  " << argv0 << " Idle.vrma Idle.gltf --pretty\n"
				  << "  " << argv0 << " collection.glb --split-objects objects/\n";
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
			else if (arg == "-s" || arg == "--split-objects")
			{
				if (i + 1 >= argc)
				{
					std::cerr << "Error: " << arg << " requires a directory argument (use \".\" for the current directory)\n";
					return false;
				}
				opts.splitDir = argv[++i];
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

	const bool splitting = !opts.splitDir.empty();

	Container outputContainer = Container::Binary;
	if (splitting)
	{
		if (!opts.outputFile.empty())
		{
			std::cerr << "Error: --split-objects writes one file per object, so it cannot be combined with an output file\n";
			return 1;
		}
	}
	else
	{
		if (opts.outputFile.empty())
			opts.outputFile = DefaultOutputPath(opts.inputFile, inputContainer);

		if (!DetectContainer(opts.outputFile, outputContainer))
		{
			std::cerr << "Error: Unrecognized output extension for: " << opts.outputFile
					  << " (expected .gltf, .glb, .vrm or .vrma)\n";
			return 1;
		}
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

	// Split mode writes one .glb per root object and never touches the single-file conversion
	// path below - in particular it must not splice the original root extensions back in, as
	// their node indices no longer refer to anything in the split objects.
	if (splitting)
		return RunSplit(model, io, opts.inputFile, opts.splitDir, opts.verbose);

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
