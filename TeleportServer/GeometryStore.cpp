#pragma optimize("",off)
#include "GeometryStore.h"
#include "TeleportCore/ErrorHandling.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <regex>
#include <fmt/core.h>
#include "basisu_comp.h"

#if defined ( _WIN32 )
#include <sys/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#endif
#include "TeleportCore/AnimationInterface.h"
#include "TeleportCore/TextCanvas.h"
#ifdef _MSC_VER
// disable Google's compiler warning.
#pragma warning(disable:4018)
#endif
#include "draco/compression/encode.h"
#include "draco/compression/decode.h"

#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Font.h"
#include "UnityPlugin/InteropStructures.h"
#include "TeleportCore/StringFunctions.h"
using namespace std::string_literals;
using namespace teleport;
using namespace server;
#ifdef _MSC_VER
//We need to use the experimental namespace if we are using MSVC 2017, but not for 2019+.
#if _MSC_VER < 1920
namespace filesystem = std::experimental::filesystem;
#else
namespace filesystem = std::filesystem;
#endif
#else
namespace filesystem = std::filesystem;
#endif

std::string StandardizePath(const std::string &file_name,const std::string &path_root)
{
	filesystem::path path(file_name);
	path = path.lexically_relative(path_root);
	path = path.replace_extension("");
	std::string p=path.generic_u8string();
	std::replace(p.begin(),p.end(),' ','%');
	std::replace(p.begin(),p.end(),'\\','/');
	std::string r=path_root;
	if(r.size()&&r[r.size()-1]!='/')
		r+="/";
	p=std::regex_replace( p, std::regex(r), "" );
	size_t last_dot_pos = p.rfind('.');
	size_t last_slash_pos = p.rfind('/');
	// Knock off the extension if it's the extension of the filename, not just a dot in a pathname...
	if(last_dot_pos<p.length()&&(last_slash_pos>=p.length()||last_slash_pos<last_dot_pos))
		p=p.substr(0,last_dot_pos);
	return p;
}
#ifdef _MSC_VER
static avs::guid bstr_to_guid(std::string b)
{
	avs::guid g;
	strncpy_s(g.txt,(const char*)b.c_str(),48);
	g.txt[48]=0;
	return g;
}

static std::string guid_to_bstr(const avs::guid &g)
{
	char txt[49];
	strncpy_s(txt,g.txt,48);
	txt[48]=0;
	std::string b(txt);
	return b;
}
#endif
template<class T>
std::vector<avs::uid> getVectorOfIDs(const std::map<avs::uid, T>& resourceMap)
{
	std::vector<avs::uid> ids(resourceMap.size());

	size_t i = 0;
	for(const auto& it : resourceMap)
	{
		ids[i++] = it.first;
	}
	return ids;
}

template<class T> T* getResource(std::map<avs::uid, T>& resourceMap, avs::uid id)
{
	auto resource = resourceMap.find(id);
	return (resource != resourceMap.end()) ? &resource->second : nullptr;
}

//Const version.
template<class T> const T* getResource(const std::map<avs::uid, T>& resourceMap, avs::uid id)
{
	auto resource = resourceMap.find(id);
	return (resource != resourceMap.end()) ? &resource->second : nullptr;
}

std::time_t GetFileWriteTime(const std::filesystem::path& filename)
{
#if defined ( _WIN32 )
	{
		struct _stat64 fileInfo;
		if(_wstati64(filename.wstring().c_str(), &fileInfo) != 0)
		{
			throw std::runtime_error("Failed to get last write time.");
		}
		return fileInfo.st_mtime;
	}
#else

#ifdef CPP20
	auto write_time=fs::last_write_time(filename.c_str());
	const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
	const auto time = std::chrono::system_clock::to_time_t(systemTime);
    return time;
#else
	struct stat buf;
	stat(filename.c_str(), &buf);
	buf.st_mtime;
	time_t t = buf.st_mtime;
	return t;
#endif

#endif
}

const char* stringOf(avs::NodeDataType type)
{
	switch(type)
	{
	case avs::NodeDataType::Invalid:	return "Invalid";
	case avs::NodeDataType::None:		return "None";
	case avs::NodeDataType::Mesh:		return "Mesh";
	case avs::NodeDataType::Light:		return "Light";
	default:							return "Unimplemented";
	};
}

GeometryStore::GeometryStore()
{
	//Create look-up maps.
	meshes[avs::AxesStandard::EngineeringStyle];
	meshes[avs::AxesStandard::GlStyle];
	animations[avs::AxesStandard::EngineeringStyle];
	animations[avs::AxesStandard::GlStyle];
	skeletons[avs::AxesStandard::EngineeringStyle];
	skeletons[avs::AxesStandard::GlStyle];
	
	uid_to_path[0]=".";
	path_to_uid["."]=0;
}

GeometryStore::~GeometryStore()
{
}
 GeometryStore &GeometryStore::GetInstance()
 {
	static GeometryStore geometryStore;
	return geometryStore;
 }

bool GeometryStore::saveToDisk() const
{
	if(!saveResourcesBinary(cachePath + "/" , textures))
		return false;
	if(!saveResourcesBinary(cachePath + "/" , materials))
		return false;
	if(!saveResourcesBinary(cachePath + "/engineering/" , meshes.at(avs::AxesStandard::EngineeringStyle)))
		return false;
	if(!saveResourcesBinary(cachePath + "/gl/" , meshes.at(avs::AxesStandard::GlStyle)))
		return false;
	return true;
}

bool GeometryStore::SetCachePath(const char* path)
{
	bool exist = std::filesystem::exists(std::filesystem::path(path));
	if (exist)
		cachePath = path;

	return exist;
}

void GeometryStore::Verify()
{
	loadResourcesBinary(cachePath , materials);
}

void GeometryStore::loadFromDisk(size_t& numMeshes
	,LoadedResource*& loadedMeshes, size_t& numTextures
	,LoadedResource*& loadedTextures, size_t& numMaterials
	,LoadedResource*& loadedMaterials)
{
	// Load in order of non-dependent to dependent resources, so that we can apply dependencies.
	loadResourcesBinary(cachePath + "/" , textures);
	loadResourcesBinary(cachePath + "/" , materials);
	loadResourcesBinary(cachePath + "/engineering/" , meshes.at(avs::AxesStandard::EngineeringStyle));
	loadResourcesBinary(cachePath + "/gl/", meshes.at(avs::AxesStandard::GlStyle));
	
	// Now fill in the return values.
	numMeshes = meshes.at(avs::AxesStandard::EngineeringStyle).size();
	numTextures = textures.size();
	numMaterials = materials.size();

	int i = 0;
	loadedMeshes = new LoadedResource[numMeshes];
	for(auto& meshDataPair : meshes.at(avs::AxesStandard::EngineeringStyle))
	{
		loadedMeshes[i] = LoadedResource(meshDataPair.first,   meshDataPair.second.path.c_str(), meshDataPair.second.mesh.name.c_str(), meshDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedTextures = new LoadedResource[numTextures];
	for(auto& textureDataPair : textures)
	{
		loadedTextures[i] = LoadedResource(textureDataPair.first,  textureDataPair.second.path.c_str(), textureDataPair.second.texture.name.c_str(), textureDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedMaterials = new LoadedResource[numMaterials];
	for(auto& materialDataPair : materials)
	{
		loadedMaterials[i] = LoadedResource(materialDataPair.first, 
			 materialDataPair.second.path.c_str(), materialDataPair.second.material.name.c_str(), materialDataPair.second.lastModified);

		++i;

#if TELEPORT_INTERNAL_CHECKS		
		std::vector<avs::uid> materialTexture_uids =materialDataPair.second.material.GetTextureUids();
		for(auto u:materialTexture_uids)
		{
			if(!getTexture(u))
			{
				TELEPORT_CERR<<"Material "<<materialDataPair.second.material.name.c_str()<<" points to "<<u<<" which is not a texture.\n";
				continue;
			}
		}
#endif
	}
}

void GeometryStore::clear(bool freeMeshBuffers)
{
	//Free memory for primitive attributes and geometry buffers.
	for(auto& standardPair : meshes)
	{
		for(auto& meshPair : standardPair.second)
		{
			for(avs::PrimitiveArray& primitive : meshPair.second.mesh.primitiveArrays)
			{
				delete[] primitive.attributes;
			}

			//Unreal just uses the pointer, but Unity copies them on the native side.
			if(freeMeshBuffers)
			{
				for(auto& bufferPair : meshPair.second.mesh.buffers)
				{
					delete[] bufferPair.second.data;
				}
			}
		}
	}

	//Free memory for texture pixel data.
	for(auto& idTexturePair : textures)
	{
		idTexturePair.second.texture.data.clear();
	}

	//Free memory for shadow map pixel data.
	for(auto& idShadowPair : shadowMaps)
	{
		idShadowPair.second.texture.data.clear();
	}

	//Clear lookup tables; we want to clear the resources inside them, not their structure.
	nodes.clear();
	skeletons[avs::AxesStandard::EngineeringStyle].clear();
	skeletons[avs::AxesStandard::GlStyle].clear();
	animations[avs::AxesStandard::EngineeringStyle].clear();
	animations[avs::AxesStandard::GlStyle].clear();
	meshes[avs::AxesStandard::EngineeringStyle].clear();
	meshes[avs::AxesStandard::GlStyle].clear();
	materials.clear();
	textures.clear();
	shadowMaps.clear();

	texturesToCompress.clear();
	lightNodes.clear();
	std::filesystem::path p(cachePath);
	for (auto const& dir_entry : std::filesystem::directory_iterator(p))
	{
		std::filesystem::remove_all(dir_entry);
	}
}
void GeometryStore::setCompressionLevels(uint8_t str, uint8_t q)
{
	compressionStrength = str;
	compressionQuality= q;
}

const char* GeometryStore::getNodeName(avs::uid nodeID) const
{
	const avs::Node* node = getNode(nodeID);
	return node ? node->name.c_str() : "NULL";
}

std::vector<avs::uid> GeometryStore::getNodeIDs() const
{
	return getVectorOfIDs(nodes);
}

avs::Node* GeometryStore::getNode(avs::uid nodeID)
{
	return getResource(nodes, nodeID);
}

const avs::Node* GeometryStore::getNode(avs::uid nodeID) const
{
	return getResource(nodes, nodeID);
}

const std::map<avs::uid, avs::Node>& GeometryStore::getNodes() const
{
	return nodes;
}

avs::Skeleton* GeometryStore::getSkeleton(avs::uid skeletonID, avs::AxesStandard standard)
{
	return getResource(skeletons.at(standard), skeletonID);
}

const avs::Skeleton* GeometryStore::getSkeleton(avs::uid skeletonID, avs::AxesStandard standard) const
{
	return getResource(skeletons.at(standard), skeletonID);
}

teleport::core::Animation* GeometryStore::getAnimation(avs::uid id, avs::AxesStandard standard)
{
	return getResource(animations.at(standard), id);
}

const teleport::core::Animation* GeometryStore::getAnimation(avs::uid id, avs::AxesStandard standard) const
{
	return getResource(animations.at(standard), id);
}

std::vector<avs::uid> GeometryStore::getMeshIDs() const
{
	//Every mesh map should be identical, so we just use the engineering style.
	return getVectorOfIDs(meshes.at(avs::AxesStandard::EngineeringStyle));
}
const ExtractedMesh* GeometryStore::getExtractedMesh(avs::uid meshID, avs::AxesStandard standard) const
{
	const ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return meshData;
}

const avs::CompressedMesh* GeometryStore::getCompressedMesh(avs::uid meshID, avs::AxesStandard standard) const
{
	const ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return (meshData ? &meshData->compressedMesh : nullptr);
}

avs::Mesh* GeometryStore::getMesh(avs::uid meshID, avs::AxesStandard standard)
{
	ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return (meshData ? &meshData->mesh : nullptr);
}

const avs::Mesh* GeometryStore::getMesh(avs::uid meshID, avs::AxesStandard standard) const
{
	const ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return (meshData ? &meshData->mesh : nullptr);
}

std::vector<avs::uid> GeometryStore::getTextureIDs() const
{
	return getVectorOfIDs(textures);
}

avs::Texture* GeometryStore::getTexture(avs::uid textureID)
{
	ExtractedTexture* textureData = getResource(textures, textureID);
	return (textureData ? &textureData->texture : nullptr);
}

const avs::Texture* GeometryStore::getTexture(avs::uid textureID) const
{
	const ExtractedTexture* textureData = getResource(textures, textureID);
	return (textureData ? &textureData->texture : nullptr);
}

std::vector<avs::uid> GeometryStore::getMaterialIDs() const
{
	return getVectorOfIDs(materials);
}

avs::Material* GeometryStore::getMaterial(avs::uid materialID)
{
	ExtractedMaterial* materialData = getResource(materials, materialID);
	return materialData ? &materialData->material : nullptr;
}

const avs::Material* GeometryStore::getMaterial(avs::uid materialID) const
{
	const ExtractedMaterial* materialData = getResource(materials, materialID);
	return materialData ? &materialData->material : nullptr;
}

std::vector<avs::uid> GeometryStore::getShadowMapIDs() const
{
	return getVectorOfIDs(shadowMaps);
}

avs::Texture* GeometryStore::getShadowMap(avs::uid shadowID)
{
	ExtractedTexture* textureData = getResource(shadowMaps, shadowID);
	return (textureData ? &textureData->texture : nullptr);
}

const avs::Texture* GeometryStore::getShadowMap(avs::uid shadowID) const
{
	const ExtractedTexture* shadowMapData = getResource(shadowMaps, shadowID);
	return (shadowMapData ? &shadowMapData->texture : nullptr);
}

const teleport::core::TextCanvas* GeometryStore::getTextCanvas(avs::uid u) const
{
	auto t=textCanvases.find(u);
	if(t==textCanvases.end())
		return nullptr;
	return &t->second;
}

const teleport::core::FontAtlas* GeometryStore::getFontAtlas(avs::uid u) const
{
	auto t=fontAtlases.find(u);
	if(t==fontAtlases.end())
		return nullptr;
	return &t->second.fontAtlas;
}

const std::map<avs::uid,avs::LightNodeResources>& GeometryStore::getLightNodes() const
{
	return lightNodes;
}

bool GeometryStore::hasNode(avs::uid id) const
{
	return nodes.find(id) != nodes.end();
}

bool GeometryStore::hasMesh(avs::uid id) const
{
	return meshes.at(avs::AxesStandard::EngineeringStyle).find(id) != meshes.at(avs::AxesStandard::EngineeringStyle).end();
}

bool GeometryStore::hasMaterial(avs::uid id) const
{
	return materials.find(id) != materials.end();
}

bool GeometryStore::hasTexture(avs::uid id) const
{
	return textures.find(id) != textures.end();
}

bool GeometryStore::hasShadowMap(avs::uid id) const
{
	return shadowMaps.find(id) != shadowMaps.end();
}

void GeometryStore::setNodeParent(avs::uid id, avs::uid parent_id, avs::Pose relPose)
{
	if (nodes.find(id) == nodes.end())
		return;
	if(parent_id==nodes[id].parentID)
		return;
	nodes[id].localTransform.position = relPose.position;
	nodes[id].localTransform.rotation = relPose.orientation;
	nodes[id].parentID = parent_id;
}
void GeometryStore::storeNode(avs::uid id, avs::Node& newNode)
{
	nodes[id] = newNode;
	if (newNode.parentID != 0)
	{
		avs::Node* parent = getNode(newNode.parentID);
	}
	if(newNode.data_type == avs::NodeDataType::Light)
	{
		lightNodes[id]=avs::LightNodeResources{id, newNode.data_uid};
	}
}

void GeometryStore::storeSkeleton(avs::uid id, avs::Skeleton& newSkeleton, avs::AxesStandard sourceStandard)
{
	skeletons[avs::AxesStandard::EngineeringStyle][id] = avs::Skeleton::convertToStandard(newSkeleton, sourceStandard, avs::AxesStandard::EngineeringStyle);
	skeletons[avs::AxesStandard::GlStyle][id] = avs::Skeleton::convertToStandard(newSkeleton, sourceStandard, avs::AxesStandard::GlStyle);
}

void GeometryStore::storeAnimation(avs::uid id, std::string path, teleport::core::Animation& animation, avs::AxesStandard sourceStandard)
{
	auto &anim1=animations[avs::AxesStandard::EngineeringStyle][id] = teleport::core::Animation::convertToStandard(animation, sourceStandard, avs::AxesStandard::EngineeringStyle);
	std::string genericFilePath = path + ".teleport_anim"s;
	saveResourceBinary(cachePath + "/engineering/"s + genericFilePath, anim1);
	auto &anim2 = animations[avs::AxesStandard::GlStyle][id] = teleport::core::Animation::convertToStandard(animation, sourceStandard, avs::AxesStandard::GlStyle);
	saveResourceBinary(cachePath + "/gl/"s + genericFilePath, anim2);
}

draco::DataType ToDracoDataType(avs::Accessor::ComponentType componentType)
{
	switch(componentType)
	{
	case avs::Accessor::ComponentType::FLOAT:
		return draco::DataType::DT_FLOAT32;
	case avs::Accessor::ComponentType::DOUBLE:
		return draco::DataType::DT_FLOAT64;
	case avs::Accessor::ComponentType::HALF:
		return draco::DataType::DT_INVALID;
	case avs::Accessor::ComponentType::UINT:
		return draco::DataType::DT_INT32;
	case avs::Accessor::ComponentType::USHORT:
		return draco::DataType::DT_INT16;
	case avs::Accessor::ComponentType::UBYTE:
		return draco::DataType::DT_INT8;
	case avs::Accessor::ComponentType::INT:
		return draco::DataType::DT_INT32;
	case avs::Accessor::ComponentType::SHORT:
		return draco::DataType::DT_INT16;
	case avs::Accessor::ComponentType::BYTE:
		return draco::DataType::DT_INT8;
	default:
		break;
	};
	return draco::DataType::DT_INVALID;
}

draco::GeometryAttribute::Type ToDracoGeometryAttribute(avs::AttributeSemantic attributeSemantic)
{
	switch (attributeSemantic)
	{
	case avs::AttributeSemantic::POSITION:
		return draco::GeometryAttribute::Type::POSITION;
	case avs::AttributeSemantic::NORMAL:
		return draco::GeometryAttribute::Type::NORMAL;
	case avs::AttributeSemantic::TANGENT:
		return draco::GeometryAttribute::Type::GENERIC;
	case avs::AttributeSemantic::TEXCOORD_0:
		return draco::GeometryAttribute::Type::TEX_COORD;
	case avs::AttributeSemantic::TEXCOORD_1:
		return draco::GeometryAttribute::Type::TEX_COORD;
	case avs::AttributeSemantic::COLOR_0:
		return draco::GeometryAttribute::Type::COLOR;
	case avs::AttributeSemantic::JOINTS_0:
		return draco::GeometryAttribute::Type::GENERIC;
	case avs::AttributeSemantic::WEIGHTS_0:
		return draco::GeometryAttribute::Type::GENERIC;
	case avs::AttributeSemantic::TANGENTNORMALXZ:
		return draco::GeometryAttribute::Type::GENERIC;
	default:
		break;
	};
	return draco::GeometryAttribute::Type::INVALID;
}

static bool CompressMesh(avs::CompressedMesh &compressedMesh,avs::Mesh &sourceMesh)
{
	compressedMesh.meshCompressionType = avs::MeshCompressionType::NONE;
	compressedMesh.name=sourceMesh.name;
	draco::Encoder dracoEncoder;
	static int encode_speed=3, decode_speed =7;
	static int pos_quantization=11;
	static int normal_quantization = 8;
	static int texc_quantization = 8;
	static int colour_quantization = 8;
	static int generic_quantization = 8;
	static bool allow_edgebreaker_encoding = true;
	dracoEncoder.SetSpeedOptions(encode_speed, decode_speed);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::POSITION, pos_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::NORMAL, normal_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::COLOR, colour_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::TEX_COORD, texc_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::GENERIC, generic_quantization);
	if (allow_edgebreaker_encoding)
		dracoEncoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
	else
		dracoEncoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
	size_t sourceSize=0;
	size_t compressedSize = 0;
	// create zero-based material indices:
	compressedMesh.subMeshes.resize(sourceMesh.primitiveArrays.size());
	// Primitive array elements in each mesh.
	draco::FaceIndex face_index(0);
	std::map<avs::uid, avs::AttributeSemantic> attributeSemantics;
	for (size_t i=0;i<sourceMesh.primitiveArrays.size();i++)
	{
		const auto& primitive = sourceMesh.primitiveArrays[i];
		auto &subMesh= compressedMesh.subMeshes[i];
		const avs::Accessor& indices_accessor = sourceMesh.accessors[primitive.indices_accessor];
		
		subMesh.material=primitive.material;
		subMesh.indices_accessor=primitive.indices_accessor;
		subMesh.first_index= 0;
		subMesh.num_indices = (uint32_t)indices_accessor.count;
		size_t numTriangles = indices_accessor.count / 3;
	
		draco::Mesh dracoMesh;
		draco::EncoderBuffer dracoEncoderBuffer;
		size_t numVertices = 0;
		for (size_t j = 0; j < primitive.attributeCount; j++)
		{
			const avs::Attribute& attrib = primitive.attributes[j];
			const auto &attrib_accessor=sourceMesh.accessors[attrib.accessor];
			numVertices=std::max(numVertices, attrib_accessor.count);
			auto s=attributeSemantics.find(attrib.accessor);
			if(s==attributeSemantics.end())
				attributeSemantics[attrib.accessor]=attrib.semantic;
			 if(attrib.semantic!= attributeSemantics[attrib.accessor])
			{
				TELEPORT_CERR<<"Different attributes for submeshes, can't compress this: "<<sourceMesh.name.c_str()<<"\n";
				return false;
			}
		}
		dracoMesh.set_num_points((uint32_t)numVertices);
		dracoMesh.SetNumFaces(numTriangles);
		for (const auto& a : sourceMesh.accessors)
		{
			const auto &accessor=a.second;
			draco::DataType dracoDataType = ToDracoDataType(accessor.componentType);
			auto s = attributeSemantics.find(a.first);
			if (s == attributeSemantics.end())
				continue;	// not an attribute.
			auto semantic=s->second;
			draco::GeometryAttribute::Type dracoGeometryAttributeType = ToDracoGeometryAttribute(semantic);
			
			avs::BufferView& bufferView = sourceMesh.bufferViews[accessor.bufferView];
			avs::GeometryBuffer& geometryBuffer = sourceMesh.buffers[bufferView.buffer];
			const uint8_t* data = geometryBuffer.data + bufferView.byteOffset;
			// Naively convert enum to integer.
			int8_t num_components = (int8_t)accessor.type;	
			int att_id = -1;
			draco::PointAttribute* attr = nullptr;
			draco::GeometryAttribute dracoGeometryAttribute;
			size_t stride = draco::DataTypeLength(dracoDataType) * num_components;
			dracoGeometryAttribute.Init(dracoGeometryAttributeType, nullptr, num_components, dracoDataType, semantic == avs::AttributeSemantic::NORMAL, stride, 0);
			att_id = dracoMesh.AddAttribute(dracoGeometryAttribute, true, (uint32_t)(geometryBuffer.byteLength / stride));
			subMesh.attributeSemantics[att_id]=semantic;
			attr = dracoMesh.attribute(att_id);
			for (size_t j = 0; j < accessor.count; j++)
			{
				attr->SetAttributeValue(draco::AttributeValueIndex((uint32_t)j), data + j * bufferView.byteStride);
			}
			sourceSize += bufferView.byteLength;
		}
		//Indices
		const avs::BufferView& indicesBufferView = sourceMesh.bufferViews[indices_accessor.bufferView];
		sourceSize += indicesBufferView.byteLength;
		const avs::GeometryBuffer& indicesBuffer = sourceMesh.buffers[indicesBufferView.buffer];
		//size_t componentSize = avs::GetComponentSize(indices_accessor.componentType);
		size_t triangleCount= indices_accessor.count / 3;
		if(indicesBufferView.byteStride==4)
		{
			uint32_t* data=(uint32_t*)(indicesBuffer.data + indicesBufferView.byteOffset+ indices_accessor.byteOffset);
			for(size_t j=0;j< triangleCount;j++)
			{
				draco::Mesh::Face dracoMeshFace;
				for(int k=0;k<3;k++)
					dracoMeshFace[k]= data[j*3+k];
				dracoMesh.SetFace(face_index,dracoMeshFace);
				++face_index;
			}
		}
		else if (indicesBufferView.byteStride == 2)
		{
			uint16_t* data = (uint16_t*)(indicesBuffer.data + indicesBufferView.byteOffset + indices_accessor.byteOffset);
			for (size_t j = 0; j < triangleCount; j++)
			{
				draco::Mesh::Face dracoMeshFace;
				for (int k = 0; k < 3; k++)
					dracoMeshFace[k] = data[j * 3 + k];
				dracoMesh.SetFace(face_index, dracoMeshFace);
				++face_index;
			}
		}
		else
		{
			TELEPORT_ASSERT(false);
		}
		//dracoMesh.DeduplicateAttributeValues();
		//dracoMesh.DeduplicatePointIds();
		
		draco::Status status= dracoEncoder.EncodeMeshToBuffer(dracoMesh,&dracoEncoderBuffer);
		if(!status.ok())
		{
			TELEPORT_INTERNAL_LOG_UNSAFE("dracoEncoder failed\n");
			return false;
		}
		subMesh.buffer.resize(dracoEncoderBuffer.size());
		memcpy(subMesh.buffer.data(), dracoEncoderBuffer.data(), subMesh.buffer.size());
		compressedSize+= subMesh.buffer.size();
		if (subMesh.buffer.size() == 0)
		{
			TELEPORT_INTERNAL_CERR("Empty compressed submesh {0}\n", sourceMesh.name.c_str());
			TELEPORT_INTERNAL_BREAK_ONCE("");
		}
	}
	//TELEPORT_INTERNAL_COUT("Compressed {0} from {1} to {2}\n",sourceMesh.name.c_str(),(sourceSize+1023)/1024,(compressedSize +1023)/1024);
	compressedMesh.meshCompressionType=avs::MeshCompressionType::DRACO_VERSIONED;
/*
	draco::Decoder dracoDecoder;
	draco::DecoderBuffer dracoDecoderBuffer;
	dracoDecoderBuffer.Init((const char*)dracoEncoderBuffer.data(), dracoEncoderBuffer.size());
	draco::Mesh dracoMesh2;
	dracoDecoder.DecodeBufferToGeometry(&dracoDecoderBuffer,&dracoMesh2);*/
	return true;
}

static bool FloatCompare(float a,float b)
{
	float diff=(a-b);
	float m=std::max(b,1.0f);
	float dif_rel=diff/m;
	if(dif_rel>0.1f)
		return false;
	return true;
}

static bool VecCompare(vec2 a, vec2 b)
{
	float A=sqrt(a.x*a.x+a.y*a.y);
	float B=sqrt(b.x*b.x+b.y*b.y);
	float m = std::max(B, 1.0f);
	m=std::max(m,A);
	vec2 diff = a - b;
	float dif_rel = abs(diff.x+diff.y)/ m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

static bool VecCompare(vec3 a, vec3 b)
{
	float B = sqrt(b.x * b.x + b.y * b.y + b.z * b.z);
	float m = std::max(B, 1.0f);
	vec3 diff = a - b;
	float dif_rel = abs(diff.x + diff.y + diff.z) / m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

static bool VecCompare(vec4 a, vec4 b)
{
	float B = sqrt(b.x * b.x + b.y * b.y + b.z * b.z + b.w * b.w);
	float m = std::max(B, 1.0f);
	vec4 diff = a - b;
	float dif_rel = abs(diff.x + diff.y + diff.z + diff.w) / m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

static bool MatCompare(mat4 a, mat4 b)
{
	float A = sqrt(a.a * a.a + a.f * a.f + a.k * a.k + a.p * a.p);
	float B = sqrt(b.a * b.a + b.f * b.f + b.k * b.k + b.p * b.p);
	float m = std::max(B, 1.0f);
	m = std::max(m, A);
	mat4 diff = a - b;
	float total=0;
	for(int i=0;i<16;i++)
	{
		total+=diff.m[i];
	}
	float dif_rel = abs(total) / m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

template<int n> bool IntCompare(const int*a,const int*b)
{
	for(size_t i=0;i<n;i++)
	{
		if(a[i]!=b[i])
			return false;
	}
	return true;
}

static bool VerifyCompressedMesh(avs::CompressedMesh& compressedMesh,const avs::Mesh& sourceMesh)
{
	for(int i=0;i<compressedMesh.subMeshes.size();i++)
	{
		auto &subMesh=compressedMesh.subMeshes[i];
		draco::Decoder dracoDecoder;
		draco::DecoderBuffer dracoDecoderBuffer;
		dracoDecoderBuffer.Init((const char*)subMesh.buffer.data(), subMesh.buffer.size());
		const auto &primitiveArray=sourceMesh.primitiveArrays[i];
		auto statusor = dracoDecoder.DecodeMeshFromBuffer(&dracoDecoderBuffer);
		auto &dm=statusor.value();
		if(!dm.get())
			return false;
		// are there the same number of members for each attrib?
		size_t num_elements=0;
		for(int a=0;a<dm->num_attributes();a++)
		{
			const draco::PointAttribute *attr=dm->attribute(a);
			if(!attr)
				continue;
			if(!num_elements)
				num_elements =attr->size();
			else if(attr->size()!= num_elements)
			{
				TELEPORT_INTERNAL_LOG_UNSAFE("Attr size mismatch %ull != %ull",attr->size(), num_elements);
			}
			const uint8_t *dracoDatabuffer=attr->GetAddress(draco::AttributeValueIndex(0));
			const auto &attribute=primitiveArray.attributes[a];
			if(attribute.semantic!=subMesh.attributeSemantics[a])
			{
				TELEPORT_INTERNAL_LOG_UNSAFE("Attr semantic mismatch");
				continue;
			}
			const avs::Accessor  &attr_accessor=sourceMesh.accessors.find(attribute.accessor).operator*().second;
			const avs::BufferView& attr_bufferView=(*(sourceMesh.bufferViews.find(attr_accessor.bufferView))).second;
			const avs::GeometryBuffer &attr_buffer=(*(sourceMesh.buffers.find(attr_bufferView.buffer))).second;
			const uint8_t *src_data=attr_buffer.data+attr_bufferView.byteOffset+attr_accessor.byteOffset; 
			for (int i = 0; i < attr->size(); i++)
			{
				if(attr_accessor.componentType==avs::Accessor::ComponentType::INT)
				{
					size_t sz=(size_t)attr_accessor.type;
					const int *compare=nullptr;
					switch(sz)
					{
						case 1:
						{
							auto val=(attr->GetValue<int, 1>(draco::AttributeValueIndex(i)));
							compare = ((const int *)&val);
							if (!IntCompare<1>(compare, (const int*)src_data))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
								break;
						case 2:
							{
								auto val=attr->GetValue<int, 2>(draco::AttributeValueIndex(i));
								compare = ((const int*)&(val));
								if (!IntCompare<1>(compare, (const int*)src_data))
								{
									TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
									break;
								}
							}
								break;
						case 3:
						{
							auto val=attr->GetValue<int, 3>(draco::AttributeValueIndex(i));
							compare = ((const int*)&(val));
							if (!IntCompare<1>(compare, (const int*)src_data))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
								break;
						case 4:
						{
							auto val=attr->GetValue<int, 4>(draco::AttributeValueIndex(i));
							compare = ((const int*)&(val));
							if (!IntCompare<1>(compare, (const int*)src_data))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
								break;
						default:
						break;
					}
					src_data+=sz*sizeof(int);
				}
				else if (attr_accessor.componentType == avs::Accessor::ComponentType::FLOAT)
				{
					switch(attr_accessor.type)
					{
						case avs::Accessor::DataType::SCALAR:
						{
							float compare = attr->GetValue<float,1>(draco::AttributeValueIndex(i))[0];
							if(!FloatCompare(compare,*((const float*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC2:
						{
							auto c= attr->GetValue<float, 2>(draco::AttributeValueIndex(i));
							vec2 compare = *((const vec2*)&c);
							if (!VecCompare(compare,*((const vec2*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC3:
						{
							auto c = attr->GetValue<float, 3>(draco::AttributeValueIndex(i));
							vec3 compare = *((const vec3*)&c);
							if (!VecCompare(compare, *((const vec3*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC4:
						{
							auto c = attr->GetValue<float, 4>(draco::AttributeValueIndex(i));
							vec4 compare = *((const vec4*)&c);
							if (!VecCompare(compare, *((const vec4*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::MAT4:
						{
							auto c = attr->GetValue<float, 16>(draco::AttributeValueIndex(i));
							mat4 compare = *((const mat4*)&c);
							if (!MatCompare(compare, *((const mat4*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						default:
							break;
					}
				}
			}
		}
#if 0
		draco::Mesh &dracoMesh=*dm;
		uint32_t max_index = 0;
		uint32_t max_value = 0;
		uint32_t unset = 0xffffffff;
		// Go through each unique point and see whether the "mapped indices" are the same for the attributes:
		for(uint32_t idx=0;idx<dracoMesh.num_points();idx++)
		{
			max_index = std::max(max_index, idx);
			//TELEPORT_INTERNAL_LOG_UNSAFE("Point %u:",idx);
			uint32_t index = unset;
			bool mismatch=false;
			for (int a = 0; a < dm->num_attributes(); a++)
			{
				const draco::PointAttribute* attr = dm->attribute(a);
				uint32_t val = attr->mapped_index(draco::PointIndex(idx)).value();
				if(a)
				{
				//	TELEPORT_INTERNAL_LOG_UNSAFE(",");
				}
			//	TELEPORT_INTERNAL_LOG_UNSAFE(" %u", val);
				max_value = std::max(max_value, val);
				if (index == unset)
					index = val;
				else if (val != index)
				{
					mismatch=true;
				}
			}
			if(mismatch)
			{
				TELEPORT_INTERNAL_LOG_UNSAFE("Mismatch");
			}
			TELEPORT_INTERNAL_LOG_UNSAFE("\n");
		}
		#endif
	}
	return true;
}

class resource_ofstream :public std::ofstream
{
protected:
	std::function<std::string(avs::uid)> uid_to_path;
public:
	std::string filename;
	resource_ofstream(const char *fn, std::function<std::string(avs::uid)> f)
		: filename(fn) 
		,std::ofstream(fn, std::ofstream::out | std::ofstream::binary)
		, uid_to_path(f)
	{
		unsetf(std::ios_base::skipws);
	}
	template<typename T>
	void writeChunk(const T& t)
	{
		write((const char*)&t, sizeof(t));
	}
	friend resource_ofstream& operator<<(resource_ofstream& stream, avs::uid u)
	{
		if (!u)
		{
			size_t sz = 0;
			stream.write((char*)&sz, sizeof(sz));
		}
		else
		{
			std::string p = stream.uid_to_path(u);
			std::replace(p.begin(), p.end(), ' ', '%');
			std::replace(p.begin(), p.end(), '\\', '/');
			stream << p;
		}
		return stream;
	}
	friend resource_ofstream& operator<<(resource_ofstream& stream, const std::string &s)
	{
		size_t sz = s.length();
		stream.write((char*)&sz, sizeof(sz)); 
		stream.write(s.data(), s.length());
		return stream;
	}
};
class resource_ifstream :public std::ifstream
{
protected:
	std::function<avs::uid(std::string)> path_to_uid;
	size_t fileSize=0;
public:
	std::string filename;
	resource_ifstream(const char* fn, std::function<avs::uid(std::string)> f)
		: filename(fn) 
		, std::ifstream(fn, resource_ifstream::in | resource_ifstream::binary)
		, path_to_uid(f)
	{
		fileSize=std::filesystem::file_size(fn);
		unsetf(std::ios_base::skipws);
	}
	template<typename T>
	void readChunk(T& t)
	{
		read((char*)&t, sizeof(t));
	}
	size_t getFileSize() const
	{
		return fileSize;
	}
	/// Get the number of bytes until the end of the file.
	size_t getBytesRemaining() 
	{
		return fileSize - (size_t)tellg();
	}
	std::vector<char> readData()
	{
		std::vector<char> fileContents((std::istreambuf_iterator<char>(*this)),
									   std::istreambuf_iterator<char>());
		return fileContents;
	}
	friend resource_ifstream& operator>>(resource_ifstream& stream, avs::uid& u)
	{
		std::string p;
		stream >> p;
		u = stream.path_to_uid(p);
		return stream;
	}
	friend resource_ifstream& operator>>(resource_ifstream& stream, std::string& s)
	{
		size_t sz = 0;
		stream.read((char*)&sz, sizeof(sz));
		s.resize(sz);
		stream.read(s.data(), s.length());
		return stream;
	}
};

void GeometryStore::storeMesh(avs::uid id,  std::string path,std::time_t lastModified, avs::Mesh& newMesh, avs::AxesStandard standard, bool compress,bool verify)
{
	std::string p=std::string(path);
	uid_to_path[id]=p;
	path_to_uid[p]=id;
	auto &mesh=meshes[standard][id] = ExtractedMesh{ path, lastModified, newMesh};
	if(!compress)
	{
		std::cerr<<"Mesh must be compressed.\n";
		return;
	}
	{
		CompressMesh(mesh.compressedMesh,mesh.mesh);
		if(verify)
		{
			//Save data to new file.
			{
				auto f=std::bind(&GeometryStore::UidToPath,this,std::placeholders::_1);
				resource_ofstream saveFile("verify.mesh", f);
				saveFile << mesh;
				saveFile.close();
			}
			resource_ifstream loadFile("verify.mesh", std::bind(&GeometryStore::PathToUid,this,std::placeholders::_1));
		
			ExtractedMesh testMesh;
			loadFile >> testMesh;
			VerifyCompressedMesh(testMesh.compressedMesh, testMesh.mesh);
		}
	}
	for (const auto& resourceData : meshes[standard])
	{
		auto &sub=resourceData.second.compressedMesh.subMeshes;
		for (auto s : sub)
		{
			if (s.buffer.size() == 0)
			{
				TELEPORT_INTERNAL_CERR("Empty submesh buffer {0}", resourceData.second.getName().c_str());
			}
		}
	}
}

bool validate_path(const std::string &p)
{
	if(p.find(' ')<p.length())
	{
		TELEPORT_CERR<<"Validation failed for path "<<p<<": paths should not contain spaces.\n";
		return false;
	}
	if (p.find('.') < p.length())
	{
		TELEPORT_CERR<<"Validation failed for path " << p << ": paths should not contain periods.\n";
		return false;
	}
	if (p.find(',') < p.length())
	{
		TELEPORT_CERR<<"Validation failed for path " << p << ": paths should not contain commas.\n";
		return false;
	}
	return true;
}

void GeometryStore::storeMaterial(avs::uid id, std::string guid,std::string path, std::time_t lastModified, avs::Material& newMaterial)
{
	if(!validate_path(path))
		return ;
	std::string p=std::string(path);
	uid_to_path[id]=p;
	path_to_uid[p]=id;
 	materials[id] = ExtractedMaterial{guid, path, lastModified, newMaterial};
}

void GeometryStore::storeTexture(avs::uid id, std::string guid,std::string path, std::time_t lastModified, avs::Texture& newTexture, bool genMips
	, bool highQualityUASTC,bool forceOverwrite)
{
	if (!validate_path(path))
		return;
	auto p=std::string(path);
	bool black = true;
	for (size_t i = 0; i < newTexture.data.size(); i++)
	{
		if (newTexture.data[i] != 0)
		{
			black = false;
			break;
		}
	}
	if (black)
	{
		TELEPORT_INTERNAL_CERR("Black texture {0}", path.c_str());
		TELEPORT_INTERNAL_BREAK_ONCE("");
	}
	uid_to_path[id]=p;
	path_to_uid[p]=id;
	uint16_t numImages=*((uint16_t*)newTexture.data.data());
	std::vector<uint32_t> imageOffsets(numImages);
	memcpy(imageOffsets.data(),newTexture.data.data()+2,sizeof(int32_t)*numImages);
	imageOffsets.push_back((uint32_t)newTexture.data.size());
	std::vector<size_t> imageSizes(numImages);
	for(size_t i=0;i<numImages;i++)
	{
		imageSizes[i]=imageOffsets[i+1]-imageOffsets[i];
		if(imageSizes[i]>newTexture.data.size())
		{
			TELEPORT_BREAK_ONCE("Bad data.");
			return;
		}
	}
	//Compress the texture with Basis Universal only if bytes per pixel is equal to 4.
	if(newTexture.compression==avs::TextureCompression::BASIS_COMPRESSED&&newTexture.bytesPerPixel != 4)
	{
		newTexture.compression=avs::TextureCompression::UNCOMPRESSED;
	}
	{
	/*	bool validFileExists = false;
		filesystem::path fsFilePath = cacheFilePath;
		if(filesystem::exists(fsFilePath))
		{
			//Read last write time.
			filesystem::file_time_type rawFileTime = filesystem::last_write_time(fsFilePath);

			//Convert to std::time_t; imprecise, but good enough.
			std::time_t basisModified = rawFileTime.time_since_epoch().count();

			//The file is valid if the basis file is younger than the texture file.
			validFileExists = basisModified >= lastModified;
		}*/
		//Otherwise, queue the texture for compression.
		 	// UASTC doesn't work from inside the dll. Unclear why.
		{
			std::shared_ptr<PrecompressedTexture> pct = std::make_shared<PrecompressedTexture>();
		
			pct->numMips=newTexture.mipCount;
			pct->genMips=genMips;
			pct->highQualityUASTC=highQualityUASTC;
			pct->textureCompression=newTexture.compression;
			pct->format = newTexture.format;
			for(size_t i=0;i<imageSizes.size();i++)
			{
				size_t offset=(size_t)imageOffsets[i];
				size_t imageSize=(size_t)imageSizes[i];
				const uint8_t *src=newTexture.data.data()+offset;
				std::vector<uint8_t> img;
				img.resize(imageSize);
				//Copy data from source, so it isn't lost.
				memcpy(img.data(), src, img.size());
				pct->images.push_back(std::move(img));
			}
 			newTexture.data.clear();
			
			texturesToCompress.emplace(id, pct);
		}
	}
	textures[id] = ExtractedTexture{guid, path, lastModified, newTexture};
}

avs::uid GeometryStore::storeFont(std::string ttf_path_utf8,std::string relative_asset_path_utf8,std::time_t lastModified,int size)
{
	avs::Texture avsTexture;
	std::replace(relative_asset_path_utf8.begin(),relative_asset_path_utf8.end(),'.','_');
	std::string cacheFontFilePath=relative_asset_path_utf8+"_font";
	std::string cacheTextureFilePath=relative_asset_path_utf8+"_texture";
	avs::uid font_atlas_uid=GetOrGenerateUid(cacheFontFilePath);
	avs::uid font_texture_uid=GetOrGenerateUid(cacheTextureFilePath);
	ExtractedFontAtlas &fa=fontAtlases[font_atlas_uid];
	std::vector<int> sizes={size};
	if(!Font::ExtractFont(fa.fontAtlas,ttf_path_utf8,(cachePath+"/"s+cacheTextureFilePath).c_str(),"ABCDEFGHIJKLMNOPQRSTUVWXYZ",avsTexture,sizes))
		return 0;
	std::filesystem::path p=std::string(ttf_path_utf8);
	saveResourceBinary(cacheFontFilePath,fa);
	loadResourceBinary(cacheFontFilePath, "",fa);
	storeTexture(font_texture_uid,"",relative_asset_path_utf8,lastModified, avsTexture, true,	 true,true);
	fa.fontAtlas.font_texture_uid=font_texture_uid;
	return font_atlas_uid;
}

avs::uid GeometryStore::storeTextCanvas( std::string relative_asset_path, const InteropTextCanvas *interopTextCanvas)
{
	if(!interopTextCanvas||!interopTextCanvas->text)
		return 0;
	avs::uid canvas_uid=GetOrGenerateUid(relative_asset_path);
	teleport::core::TextCanvas &textCanvas=textCanvases[canvas_uid];

	textCanvas.text=interopTextCanvas->text;
	std::string cacheFontFilePath=std::string(interopTextCanvas->font)+".font";
	avs::uid font_uid=PathToUid(cacheFontFilePath);
	if(!font_uid)
		return 0;
	auto *f=getFontAtlas(font_uid);
	if(!f)
		return 0;
	textCanvas.font_uid=font_uid;
	textCanvas.lineHeight=interopTextCanvas->lineHeight;
	textCanvas.height=interopTextCanvas->height;
	textCanvas.width=interopTextCanvas->width;
	textCanvas.size=interopTextCanvas->size;
	textCanvas.colour=interopTextCanvas->colour;
	return canvas_uid;
}

void GeometryStore::storeShadowMap(avs::uid id, std::string guid,std::string path, std::time_t lastModified, avs::Texture& newShadowMap)
{
	shadowMaps[id] = ExtractedTexture{ guid,path, lastModified, newShadowMap};
}

void GeometryStore::removeNode(avs::uid id)
{
	nodes.erase(id);
	lightNodes.erase(id);
}

void GeometryStore::updateNodeTransform(avs::uid id, avs::Transform& newLocalTransform)
{
	auto nodeIt = nodes.find(id);
	if(nodeIt == nodes.end())
		return;

	nodeIt->second.localTransform = newLocalTransform;
	//nodeIt->second.//globalTransform= newGlobalTransform;
}

size_t GeometryStore::getNumberOfTexturesWaitingForCompression() const
{
	return texturesToCompress.size();
}

const avs::Texture* GeometryStore::getNextTextureToCompress() const
{
	//No textures to compress.
	if(texturesToCompress.size() == 0)
		return nullptr;
	auto compressionPair = texturesToCompress.begin();
	auto foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());

	return &foundTexture->second.texture;
}
//#define STB_IMAGE_WRITE_IMPLEMENTATION 1
#pragma warning(disable:4996)
namespace teleport
{
#include "stb_image_write.h"
}
void GeometryStore::compressNextTexture()
{
	//No textures to compress.
	if(texturesToCompress.size() == 0)
		return;
	auto compressionPair = texturesToCompress.begin();
	auto foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());
	ExtractedTexture& extractedTexture = foundTexture->second;
	avs::Texture& avsTexture = extractedTexture.texture;
	std::shared_ptr<PrecompressedTexture> compressionData = compressionPair->second;
	std::string file_name = (cachePath + "/") + extractedTexture.MakeFilename();
	if (compressionData->textureCompression == avs::TextureCompression::BASIS_COMPRESSED)
	{
		if (!ApplyBasisCompression(extractedTexture, compressionData, compressionStrength,  compressionQuality))
		{
			texturesToCompress.erase(texturesToCompress.begin());
			return ;
		}
	}
	else 
	{
		size_t n = 0;
		bool breakout = false;
		size_t imagesPerMip = compressionData->images.size() / compressionData->numMips;
		std::vector<std::vector<uint8_t>> subImages;
		subImages.resize(compressionData->images.size());
		if (compressionData->textureCompression == avs::TextureCompression::PNG)
		{
			// encode each sub-image as a separate png.
			for (size_t i = 0; i < imagesPerMip; i++)
			{
				if (breakout)
					break;
				int w = avsTexture.width;
				int h = avsTexture.height;
				for (size_t m = 0; m < compressionData->numMips; m++)
				{
					std::vector<uint8_t> &img = compressionData->images[n];
					if (img.size() > 4)
					{
						std::string pngString = "XXX";
						memcpy(pngString.data(), img.data() + 1, 3);
						if (pngString == "PNG")
						{
							TELEPORT_CERR << "Texture " << extractedTexture.getName() << " was already a PNG, can't further compress this.\n ";
							breakout = true;
							break;
						}
					}

					auto write_func=[](void *context, void *data, int size)
					{
						std::vector<uint8_t> &subImage = *((std::vector<uint8_t>*)context);
						subImage.resize(size);
						unsigned char *target = subImage.data();
						memcpy(target, data, size);
					};

					//STBIWDEF int stbi_write_png_to_func(stbi_write_func * func, void *context, int w, int h, int comp, const void *data, int stride_in_bytes);

					int res = stbi_write_png_to_func(write_func, &subImages[n], w, h, 4, (const unsigned char *)(img.data()), w * 4);
					if (!res)
					{
						TELEPORT_CERR << "Texture " << extractedTexture.getName() << " was already a PNG, can't further compress this.\n ";
						breakout = true;
						break;
					}
					n++;
					w = (w + 1) / 2;
					h = (h + 1) / 2;
				}
			}
		}
		else  //uncompressed
		{
			subImages=compressionData->images;
		}
		uint16_t imageCount = subImages.size();
		uint32_t dataSize=(sizeof(uint16_t) + imageCount * sizeof(size_t));
		for(const auto &img:subImages)
		{
			dataSize+=img.size();
		}
		avsTexture.data.resize(dataSize);
		uint8_t *target = avsTexture.data.data();
		memcpy(target, &imageCount, sizeof(imageCount));
		target+=sizeof(imageCount);
		uint32_t offset=sizeof(imageCount)+imageCount*sizeof(uint32_t);
		std::vector<uint32_t> offsets(imageCount);
		for (size_t i=0;i<imageCount;i++)
		{
			const auto &img = subImages[i];
			uint32_t sz = (uint32_t)img.size();
			offsets[i] = offset;
			offset+=sz;
		}
		memcpy(target, offsets.data(), imageCount * sizeof(uint32_t));
		target += imageCount * sizeof(uint32_t);
		for (const auto &img : subImages)
		{
			memcpy(target, img.data(), img.size());
			target += img.size();
		}
	}
	if (compressionData->textureCompression == avs::TextureCompression::UNCOMPRESSED && avsTexture.data.size() > 1048576)
	{
		TELEPORT_WARN("Texture \"{0}\" was stored UNCOMPRESSED with a data size larger than 1MB! Size: {1}B ({2}MB).\n",avsTexture.name, avsTexture.data.size(),avsTexture.data.size() / 1048576.0f);
	}
	saveResourceBinary(file_name, extractedTexture);
	
	texturesToCompress.erase(texturesToCompress.begin());
}

template<typename ExtractedResource> bool GeometryStore::saveResourceBinary(const std::string file_name, const ExtractedResource& resource) const
{
	bool oldFileExists = filesystem::exists(file_name);

	//Rename old file.
	if (oldFileExists)
		filesystem::rename(file_name, file_name + ".bak");
	else
	{
		const std::filesystem::path fspath{ file_name.c_str() };
		auto p=fspath.parent_path();
		std::filesystem::create_directories(p);
	}
	//Save data to new file.
	auto UidToPath = std::bind(&GeometryStore::UidToPath, this, std::placeholders::_1);
	auto PathToUid = std::bind(&GeometryStore::PathToUid, this, std::placeholders::_1);
	{
		resource_ofstream resourceFile(file_name.c_str(), UidToPath);
		try
		{
			resourceFile << resource;
		}
		catch(...)
		{
			resourceFile.close();
			filesystem::remove(file_name);
			TELEPORT_CERR << "Failed to save \"" << file_name << "\"!\n";
			filesystem::rename(file_name+ ".bak", file_name );
			return false;
		}
		resourceFile.close();
	}
	// verify:
	{
		resource_ifstream verifyFile(file_name.c_str(), PathToUid);
		ExtractedResource verifyResource;
		verifyFile>>verifyResource;
		verifyFile.close();
		if(!resource.Verify(verifyResource))
		{
			TELEPORT_CERR<<"File Verification failed for "<<file_name.c_str()<<"\n";
			teleport::DebugBreak();
			{
				resource_ofstream saveFile(file_name.c_str(), UidToPath);
				saveFile << resource;
			}
			resource_ifstream verifyFile(file_name.c_str(), PathToUid);
			ExtractedResource  verifyResource2;
			verifyFile>>verifyResource2;
			verifyFile.close();
			resource.Verify(verifyResource2);
			return false;
		}
	}
	//Delete old file.
	if (oldFileExists)
		filesystem::remove(file_name + ".bak");
	return true;
}

template<typename ExtractedResource>
avs::uid GeometryStore::loadResourceBinary(const std::string file_name, const std::string& path_root, std::map<avs::uid, ExtractedResource>& resourceMap)
{
	resource_ifstream resourceFile(file_name.c_str(), std::bind(&GeometryStore::PathToUid, this, std::placeholders::_1));
	std::string p = StandardizePath(file_name, path_root);
	if(!validate_path(p))
	{
		TELEPORT_CERR << "Failed to load " << file_name.c_str() << " as resource path is not valid.\n";
		return 0;
	}
	size_t ext_pos = p.find(ExtractedResource::fileExtension());
	if (ext_pos < p.length())
		p = p.substr(0, ext_pos);
	auto write_time = std::filesystem::last_write_time(file_name);
	// If there's a duplicate, use the newer file.
	// This guid might already exist!
	avs::uid newID = 0;
	auto u = path_to_uid.find(p);
	if (u != path_to_uid.end())
	{
		newID = u->second;
	}
	else
	{
		newID = avs::GenerateUid();
	}
	ExtractedResource& newResource = resourceMap[newID];
	try
	{
		resourceFile >> newResource;
		//TELEPORT_COUT<<"Loaded Resource "<<newResource.getName().c_str()<<" from file "<<file_name.c_str()<<"\n";
	}
	catch (...)
	{
		TELEPORT_CERR << "Failed to load " << file_name.c_str() << "\n";
		try
		{
			resourceFile >> newResource;
		}
		catch (...)
		{
			return 0;
		}
		return 0;
	}
	if(!newResource.IsValid())
	{
		resourceMap.erase(newID);
		TELEPORT_CERR << "Resource: " << file_name.c_str() << " is invalid, not loading.\n";
		return 0;
	}
	uid_to_path[newID] = p;
	path_to_uid[p] = newID;
	return newID;
}
template<typename ExtractedResource> bool GeometryStore::loadResourceBinary(const std::string file_name,const std::string &path_root, ExtractedResource &resource)
{
	resource_ifstream resourceFile(file_name.c_str(), std::bind(&GeometryStore::PathToUid, this, std::placeholders::_1));
	//auto write_time= std::filesystem::last_write_time(file_name);
	try
	{
		operator>>(resourceFile,resource);
	}
	catch(...)
	{
		TELEPORT_CERR<<"Failed to load "<<file_name.c_str()<<"\n";
		return false;
	}
	return true;
}

template<typename ExtractedResource> bool GeometryStore::saveResourcesBinary(const std::string path, const std::map<avs::uid, ExtractedResource>& resourceMap) const
{
	const std::filesystem::path fspath{ path.c_str() };
	std::filesystem::create_directories(fspath);
	for (const auto& resourceData : resourceMap)
	{
		if (resourceData.second.path.length() == 0)
			continue;
		std::string file_name = (path + "/") + resourceData.second.MakeFilename();
		saveResourceBinary(file_name, resourceData.second);
	}
	return true;
}

template<typename ExtractedResource> void GeometryStore::loadResourcesBinary(const std::string path, std::map<avs::uid, ExtractedResource>& resourceMap)
{
	//Load resources if the file exists.
	const std::filesystem::path fspath{ path.c_str() };
	std::filesystem::create_directories(fspath);
	std::string search_str = ExtractedResource::fileExtension();
	std::map<avs::uid, std::filesystem::file_time_type> timestamps;
	for (auto const& dir_entry : std::filesystem::recursive_directory_iterator{ fspath })
	{
		std::string extn= dir_entry.path().extension().u8string();
		if (extn.length()>1&&search_str.find(extn) < search_str.length())
		{
			std::string file_name = dir_entry.path().string();
			if (filesystem::exists(file_name))
			{
				loadResourceBinary(file_name, path, resourceMap);
			}
		}
	}
}


bool GeometryStore::CheckForErrors() 
{
	for(auto &t:textures)
	{
		ExtractedTexture& textureData = t.second;
		
		if(textureData.texture.data.size()==0)
		{
			TELEPORT_CERR<<"Texture "<<t.second.getName()<<" is empty.\n";
			return false;
		}

	}
	return true;
}

avs::uid GeometryStore::GetOrGenerateUid(const std::string &path)
{
	std::string p = path;
	if (p.size() < 2)
		return 0;
	if (!validate_path(path))
	{
		TELEPORT_BREAK_ONCE("Failed to generate Uid for invalid path.\n");
		return 0;
	}
	if(p.find(".")<p.length())
	{
		TELEPORT_CERR<<"Nonstandard path "<<p<<", paths should not contain periods.\n";
		return 0;
	}
	if (p.find(",") < p.length())
	{
		TELEPORT_CERR << "Nonstandard path " << p << ", paths should not contain commas.\n";
		return 0;
	}
	p = StandardizePath(p, "");
	auto i=path_to_uid.find(p);
	if(i!=path_to_uid.end())
	{
		return i->second;
	}
	avs::uid uid=avs::GenerateUid();
	uid_to_path[uid]=p;
	path_to_uid[p]=uid;
	return uid;
}

avs::uid GeometryStore::PathToUid(std::string p) const
{
	p = StandardizePath(p, "");
	if (p.size() < 2)
		return 0;
	auto i = path_to_uid.find(p);
	if (i == path_to_uid.end())
	{
		TELEPORT_INTERNAL_BREAK_ONCE("No uid for this path.");
		return 0;
	}
	return i->second;
}

std::string GeometryStore::UidToPath(avs::uid u) const
{
	auto i = uid_to_path.find(u);
	if (i == uid_to_path.end())
	{
		TELEPORT_INTERNAL_BREAK_ONCE("No path for this uid.");
		return "";
	}
	return i->second;
}

bool GeometryStore::EnsureResourceIsLoaded(avs::uid u)
{
	if (textures.find(u) != textures.end())
		return true;
	if (materials.find(u) != materials.end())
		return true;
	auto &e=meshes.at(avs::AxesStandard::EngineeringStyle);
	if (e.find(u) != e.end())
		return true;
	auto &a = animations.at(avs::AxesStandard::EngineeringStyle);
	if(a.find(u)!=a.end())
		return true;
	std::string path=UidToPath(u);
	if(!path.length())
		return false;
	if(LoadResourceAtPath(path,u))
		return true;
	return false;
}

avs::uid GeometryStore::LoadResourceFromFile(std::string filename, avs::uid u)
{
	std::filesystem::path path(filename);
	std::string ext=path.extension().u8string();
	std::filesystem::path rel=path.lexically_relative(cachePath);
	avs::AxesStandard axesStandard=avs::AxesStandard::EngineeringStyle;
	std::string path_root=cachePath;
	if (rel.generic_string().find("engineering/") == 0)
	{
		rel = rel.lexically_relative("engineering");
		path_root+="/engineering/";
	}
	if (rel.generic_string().find("gl/") == 0)
	{
		rel = rel.lexically_relative("gl");
		path_root += "/gl/";
		axesStandard = avs::AxesStandard::GlStyle;
	}
	std::string fn=rel.u8string();
	if(ext==".teleport_anim")
	{
		if(loadResourceBinary(filename,path_root,animations.at(axesStandard))!=u)
		{
			TELEPORT_WARN("uid mismatch");
			return 0;
		}
		else
		{
			return u;
		}
	}
	else
	{
		TELEPORT_WARN("Not implemented");
	}
	return 0;
}

	// Load in order of non-dependent to dependent resources, so that we can apply dependencies.
//loadResourcesBinary(cachePath + "/", textures);
//loadResourcesBinary(cachePath + "/", materials);
//loadResourcesBinary(cachePath + "/engineering/", meshes.at(avs::AxesStandard::EngineeringStyle));
//loadResourcesBinary(cachePath + "/gl/", meshes.at(avs::AxesStandard::GlStyle));
//
//loadResourceBinary(file_name, path, resourceMap);

bool GeometryStore::LoadResourceAtPath(std::string p,avs::uid u)
{
// The resource type depends on the file extension.
	std::string filename = cachePath + "/"s + p;
	std::string filename_eng = cachePath + "/engineering/"s + p;
	std::string filename_gl = cachePath + "/gl/"s + p;
	// Any file will be filename + ".extension".
	// The extensions can be: .mesh, .material, .teleport_anim, .basis, .texture
	bool result=false;
	if (std::filesystem::exists(filename_eng + ".mesh"s) || std::filesystem::exists(filename_gl + ".mesh"s))
	{
		LoadResourceFromFile(filename_eng + ".mesh"s,u);
		LoadResourceFromFile(filename_gl + ".mesh"s, u);
		result = true;
	}
	if (std::filesystem::exists(filename_eng + ".teleport_anim"s) || std::filesystem::exists(filename_gl + ".teleport_anim"s))
	{
		LoadResourceFromFile(filename_eng + ".teleport_anim"s, u);
		LoadResourceFromFile(filename_gl + ".teleport_anim"s, u);
		result = true;
	}
	if (std::filesystem::exists(filename + ".material"s))
	{
		if (LoadResourceFromFile(filename + ".material"s, u))
			result = true;
	}
	if (std::filesystem::exists(filename + ".basis"s))
	{
		if (LoadResourceFromFile(filename + ".basis"s, u))
			result= true;
	}
	if (std::filesystem::exists(filename + ".texture"s))
	{
		if (LoadResourceFromFile(filename + ".texture"s, u))
			result = true;
	}
	#if 0
	const std::filesystem::directory_iterator end;
	try
	{
		std::filesystem::path p(filename);
		std::string regex = filename+"\..*$";
		for (std::filesystem::directory_iterator iter{p.parent_path()}; iter != end; iter++)
		{
			const std::string file_ext = iter->path().extension().string();
			if (std::filesystem::is_regular_file(*iter))
			{
				if (std::regex_match(file_ext, regex))
				{
					LoadResourceFromFile(iter->path().string());
					result = true;
				}
			}
		}
	}
	catch (std::exception &)
	{
	}
#endif
	return result;

}