#include "GeometryStore.h"
#include "TeleportCore/ErrorHandling.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

#if defined ( _WIN32 )
#include <sys/stat.h>
#endif

#include "TeleportCore/AnimationInterface.h"
#include "TeleportCore/TextCanvas.h"
#ifdef _MSC_VER
// disable Google's compiler warning.
#pragma warning(disable:4018)
#endif
#include "draco/compression/encode.h"
#include "draco/compression/decode.h"

#include "Platform/Shaders/SL/CppSl.sl"
#include "Font.h"
#include "UnityPlugin/InteropStructures.h"

using namespace std::string_literals;
using namespace teleport;

//We need to use the experimental namespace if we are using MSVC 2017, but not for 2019+.
#if _MSC_VER < 1920
namespace filesystem = std::experimental::filesystem;
#else
namespace filesystem = std::filesystem;
#endif

#include <regex>
std::string StandardizePath(const std::string &file_name,const std::string &path_root)
{
	std::string p=file_name;
	std::replace(p.begin(),p.end(),' ','%');
	std::replace(p.begin(),p.end(),'\\','/');
	std::string r=path_root;
	if(r.size()&&r[r.size()-1]!='/')
		r+="/";
	p=std::regex_replace( p, std::regex(r), "" );
	//size_t last_dot_pos=p.find_last_of('.');
	//size_t last_slash_pos=p.find_last_of('/');
	// Knock off the extension if it's the extension of the filename, not just a dot in a pathname...
	//if(last_dot_pos<p.length()&&(last_slash_pos>=p.length()||last_slash_pos<last_dot_pos))
	//	p=p.substr(0,last_dot_pos);
	return p;
}

static avs::guid bstr_to_guid(_bstr_t b)
{
	avs::guid g;
	strncpy_s(g.txt,(const char*)b,48);
	g.txt[48]=0;
	return g;
}

static _bstr_t guid_to_bstr(const avs::guid &g)
{
	char txt[49];
	strncpy_s(txt,g.txt,48);
	txt[48]=0;
	_bstr_t b(txt);
	return b;
}

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
	{
		auto fsTime = std::filesystem::last_write_time(filename);
		return decltype (fsTime)::clock::to_time_t(fsTime);
	}
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
	case avs::NodeDataType::Bone:		return "Bone";
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
	skins[avs::AxesStandard::EngineeringStyle];
	skins[avs::AxesStandard::GlStyle];
	
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
	if(!saveResources(cachePath + "/" , textures))
		return false;
	if(!saveResources(cachePath + "/" , materials))
		return false;
	if(!saveResources(cachePath + "/engineering/" , meshes.at(avs::AxesStandard::EngineeringStyle)))
		return false;
	if(!saveResources(cachePath + "/gl/" , meshes.at(avs::AxesStandard::GlStyle)))
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

void GeometryStore::verify()
{
	loadResources(cachePath , materials);
}

void GeometryStore::loadFromDisk(size_t& numMeshes, LoadedResource*& loadedMeshes, size_t& numTextures, LoadedResource*& loadedTextures, size_t& numMaterials, LoadedResource*& loadedMaterials)
{
	// Load in order of non-dependent to dependent resources, so that we can apply dependencies.
	loadResources(cachePath + "/" , textures);
	loadResources(cachePath + "/" , materials);
	loadResources(cachePath + "/engineering/" , meshes.at(avs::AxesStandard::EngineeringStyle));
	loadResources(cachePath + "/gl/", meshes.at(avs::AxesStandard::GlStyle));
	
	// Now fill in the return values.
	numMeshes = meshes.at(avs::AxesStandard::EngineeringStyle).size();
	numTextures = textures.size();
	numMaterials = materials.size();

	int i = 0;
	loadedMeshes = new LoadedResource[numMeshes];
	for(auto& meshDataPair : meshes.at(avs::AxesStandard::EngineeringStyle))
	{
		BSTR meshName = _com_util::ConvertStringToBSTR(meshDataPair.second.mesh.name.c_str());
		loadedMeshes[i] = LoadedResource(meshDataPair.first, meshDataPair.second.path, meshName, meshDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedTextures = new LoadedResource[numTextures];
	for(auto& textureDataPair : textures)
	{
		BSTR textureName = _com_util::ConvertStringToBSTR(textureDataPair.second.texture.name.c_str());
		loadedTextures[i] = LoadedResource(textureDataPair.first, textureDataPair.second.path, textureName, textureDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedMaterials = new LoadedResource[numMaterials];
	for(auto& materialDataPair : materials)
	{
		BSTR materialName = _com_util::ConvertStringToBSTR(materialDataPair.second.material.name.c_str());
		loadedMaterials[i] = LoadedResource(materialDataPair.first, materialDataPair.second.path, materialName, materialDataPair.second.lastModified);

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
#if TELEPORT_INTERNAL_CHECKS		
	GetClashingUids();
#endif
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
		delete[] idTexturePair.second.texture.data;
	}

	//Free memory for shadow map pixel data.
	for(auto& idShadowPair : shadowMaps)
	{
		delete[] idShadowPair.second.texture.data;
	}

	//Clear lookup tables; we want to clear the resources inside them, not their structure.
	nodes.clear();
	skins[avs::AxesStandard::EngineeringStyle].clear();
	skins[avs::AxesStandard::GlStyle].clear();
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

avs::Skin* GeometryStore::getSkin(avs::uid skinID, avs::AxesStandard standard)
{
	return getResource(skins.at(standard), skinID);
}

const avs::Skin* GeometryStore::getSkin(avs::uid skinID, avs::AxesStandard standard) const
{
	return getResource(skins.at(standard), skinID);
}

avs::Animation* GeometryStore::getAnimation(avs::uid id, avs::AxesStandard standard)
{
	return getResource(animations.at(standard), id);
}

const avs::Animation* GeometryStore::getAnimation(avs::uid id, avs::AxesStandard standard) const
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

const teleport::TextCanvas* GeometryStore::getTextCanvas(avs::uid u) const
{
	auto t=textCanvases.find(u);
	if(t==textCanvases.end())
		return nullptr;
	return &t->second;
}

const teleport::FontAtlas* GeometryStore::getFontAtlas(avs::uid u) const
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

void GeometryStore::storeNode(avs::uid id, avs::Node& newNode)
{
	nodes[id] = newNode;
	if (newNode.parentID != 0)
	{
		avs::Node* parent = getNode(newNode.parentID);
		if (parent)
		{
			if (std::find(parent->childrenIDs.begin(), parent->childrenIDs.end(),id) == parent->childrenIDs.end())
				parent->childrenIDs.push_back(id);
		}
	}
	for(auto c:newNode.childrenIDs)
	{
		avs::Node* child = getNode(c);
		if (child)
		{
			if (child->parentID != 0 && child->parentID != id)
			{
				TELEPORT_CERR << "Changing parent of " << child->name.c_str() << " to " << newNode.name.c_str() << "\n";
			}
			child->parentID = id;
		}
	}
	if(newNode.data_type == avs::NodeDataType::Light)
	{
		lightNodes[id]=avs::LightNodeResources{id, newNode.data_uid};
	}
}

void GeometryStore::storeSkin(avs::uid id, avs::Skin& newSkin, avs::AxesStandard sourceStandard)
{
	skins[avs::AxesStandard::EngineeringStyle][id] = avs::Skin::convertToStandard(newSkin, sourceStandard, avs::AxesStandard::EngineeringStyle);
	skins[avs::AxesStandard::GlStyle][id] = avs::Skin::convertToStandard(newSkin, sourceStandard, avs::AxesStandard::GlStyle);
}

void GeometryStore::storeAnimation(avs::uid id, avs::Animation& animation, avs::AxesStandard sourceStandard)
{
	animations[avs::AxesStandard::EngineeringStyle][id] = avs::Animation::convertToStandard(animation, sourceStandard, avs::AxesStandard::EngineeringStyle);
	animations[avs::AxesStandard::GlStyle][id] = avs::Animation::convertToStandard(animation, sourceStandard, avs::AxesStandard::GlStyle);
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
		size_t componentSize = avs::GetComponentSize(indices_accessor.componentType);
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
	}
	TELEPORT_INTERNAL_COUT("Compressed {0} from {1} to {2}\n",sourceMesh.name.c_str(),(sourceSize+1023)/1024,(compressedSize +1023)/1024);
	compressedMesh.meshCompressionType=avs::MeshCompressionType::DRACO;
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

static bool VecCompare(avs::vec2 a, avs::vec2 b)
{
	float A=sqrt(a.x*a.x+a.y*a.y);
	float B=sqrt(b.x*b.x+b.y*b.y);
	float m = std::max(B, 1.0f);
	avs::vec2 diff = a - b;
	float dif_rel = abs(diff.x+diff.y)/ m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

static bool VecCompare(avs::vec3 a, avs::vec3 b)
{
	float B = sqrt(b.x * b.x + b.y * b.y + b.z * b.z);
	float m = std::max(B, 1.0f);
	avs::vec3 diff = a - b;
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
						compare = ((const int *)&(attr->GetValue<int, 1>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					case 2:
						compare = ((const int*)&(attr->GetValue<int, 2>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					case 3:
						compare = ((const int*)&(attr->GetValue<int, 3>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					case 4:
						compare = ((const int*)&(attr->GetValue<int, 4>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
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
							avs::vec2 compare = *((const avs::vec2*)&c);
							if (!VecCompare(compare,*((const avs::vec2*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC3:
						{
							auto c = attr->GetValue<float, 3>(draco::AttributeValueIndex(i));
							avs::vec3 compare = *((const avs::vec3*)&c);
							if (!VecCompare(compare, *((const avs::vec3*)src_data)))
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

// Here we implement special cases of std::wofstream and wifstream that are able to convert guid_to_uid to guids and vice versa.
std::string WStringToString(const std::wstring &text)
{
	size_t origsize = text.length()+ 1;
	const size_t newsize = origsize;
	char *cstring=new char[newsize];
	
#ifdef _MSC_VER
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, cstring, (size_t)origsize, text.c_str(), (size_t)newsize );
#else
			wcstombs(cstring, text.c_str(), (size_t)newsize );
#endif
	std::string str;
	str=std::string(cstring);
	delete [] cstring;
	return str;
}
std::wstring StringToWString(const std::string &text)
{
	size_t origsize = strlen(text.c_str()) + 1;
	const size_t newsize = origsize;
	wchar_t *wcstring=new wchar_t[newsize+2];
#ifdef _MSC_VER
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wcstring, origsize, text.c_str(), _TRUNCATE);
#else
			mbstowcs(wcstring,text.c_str(),origsize);
#endif
	std::wstring str(wcstring);
	delete [] wcstring;
	return str;
}

class resource_ofstream:public std::wofstream
{
protected:
	std::function<std::string(avs::uid)> uid_to_path;
public:
	resource_ofstream(const char *filename,std::function<std::string(avs::uid)> f)
		:std::wofstream(filename, std::wofstream::out | std::wofstream::binary)
		,uid_to_path(f)
	{
	}
	friend resource_ofstream& operator<<(resource_ofstream& stream, avs::uid u)
	{
		if(!u)
		{
			stream<<L". ";
		}
		else
		{
			std::string p=stream.uid_to_path(u);
			std::replace(p.begin(),p.end(),' ','%');
			std::replace(p.begin(),p.end(),'\\','/');
			std::wstring w=StringToWString(p);
			stream<<w.c_str()<<L" ";
		}
		return stream;//<<w.c_str();//<<L" ";
	}
};

void standardize_path(std::string &p)
{
	std::replace(p.begin(),p.end(),' ','%');
}

class resource_ifstream:public std::wifstream
{
protected:
	std::function<avs::uid(std::string)> path_to_uid;
public:
	resource_ifstream(const char *filename,std::function<avs::uid(std::string)> f)
		:std::wifstream(filename, resource_ifstream::in | resource_ifstream::binary)
		,path_to_uid(f)
	{
	}
	friend resource_ifstream& operator>>(resource_ifstream& stream, avs::uid &u)
	{
		std::wstring w;
		stream>>w;
		std::string p = WStringToString(w);
		standardize_path(p);
		u=stream.path_to_uid(p);
		return stream;
	}
};

void GeometryStore::storeMesh(avs::uid id, _bstr_t guid, _bstr_t path,std::time_t lastModified, avs::Mesh& newMesh, avs::AxesStandard standard, bool compress,bool verify)
{
	std::string p=std::string(path);
	standardize_path(p);
	uid_to_path[id]=p;
	path_to_uid[p]=id;
	auto &mesh=meshes[standard][id] = ExtractedMesh{guid, path, lastModified, newMesh};
	if(compress)
	{
		CompressMesh(mesh.compressedMesh,mesh.mesh);
		if(verify)
		{
			//Save data to new file.
			{
				auto f=std::bind(&GeometryStore::UidToPath,this,std::placeholders::_1);
				resource_ofstream saveFile("verify.mesh", f);
				saveFile << mesh << "\n";
				saveFile.close();
			}
			resource_ifstream loadFile("verify.mesh", std::bind(&GeometryStore::PathToUid,this,std::placeholders::_1));
		
			ExtractedMesh testMesh;
			loadFile >> testMesh;
			VerifyCompressedMesh(testMesh.compressedMesh, testMesh.mesh);
		}
	}
}

template<typename ExtractedResource> std::string MakeResourceFilename(ExtractedResource& resource)
{
		std::string file_name;
		file_name+=resource.path+resource.fileExtension();
		return file_name;
}

void GeometryStore::storeMaterial(avs::uid id, _bstr_t guid,_bstr_t path, std::time_t lastModified, avs::Material& newMaterial)
{
	std::string p=std::string(path);
	standardize_path(p);
	uid_to_path[id]=p;
	path_to_uid[p]=id;
 	materials[id] = ExtractedMaterial{guid, path, lastModified, newMaterial};
}

void GeometryStore::storeTexture(avs::uid id, _bstr_t guid,_bstr_t path, std::time_t lastModified, avs::Texture& newTexture, std::string cacheFilePath, bool genMips
	, bool highQualityUASTC,bool forceOverwrite)
{
	auto p=std::string(path);
	standardize_path(p);
	uid_to_path[id]=p;
	path_to_uid[p]=id;
	uint16_t numImages=*((uint16_t*)newTexture.data);
	std::vector<uint32_t> imageOffsets(numImages);
	memcpy(imageOffsets.data(),newTexture.data+2,sizeof(int32_t)*numImages);
	imageOffsets.push_back(newTexture.dataSize);
	std::vector<size_t> imageSizes(numImages);
	for(size_t i=0;i<numImages;i++)
	{
		imageSizes[i]=imageOffsets[i+1]-imageOffsets[i];
		if(imageSizes[i]>newTexture.dataSize)
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
	if(!cacheFilePath.empty() )
	{
		bool validFileExists = false;
		filesystem::path fsFilePath = cacheFilePath;
		if(filesystem::exists(fsFilePath))
		{
			//Read last write time.
			filesystem::file_time_type rawFileTime = filesystem::last_write_time(fsFilePath);

			//Convert to std::time_t; imprecise, but good enough.
			std::time_t basisModified = rawFileTime.time_since_epoch().count();

			//The file is valid if the basis file is younger than the texture file.
			validFileExists = basisModified >= lastModified;
		}
		// if it isn't to be compressed or it's already been, just take the data we've been given. But copy it, because the original pointer is not ours.
		if(newTexture.compressed==true||newTexture.compression==avs::TextureCompression::UNCOMPRESSED)
		{
			uint8_t *newDataCopy = new unsigned char[newTexture.dataSize];
			memcpy(newDataCopy,newTexture.data,newTexture.dataSize);
			newTexture.data = newDataCopy;
		}
		//Otherwise, queue the texture for compression.
		else 	// UASTC doesn't work from inside the dll. Unclear why.
		{
			PrecompressedTexture pct;
			pct.basisFilePath=cacheFilePath;
			pct.numMips=newTexture.mipCount;
			pct.genMips=genMips;
			pct.highQualityUASTC=highQualityUASTC;
			pct.textureCompression=newTexture.compression;
			for(size_t i=0;i<imageSizes.size();i++)
			{
				size_t offset=(size_t)imageOffsets[i];
				size_t imageSize=(size_t)imageSizes[i];
				uint8_t *src=newTexture.data+offset;
				std::vector<uint8_t> img;
				img.resize(imageSize);
				//Copy data from source, so it isn't lost.
				memcpy(img.data(), src, img.size());
				pct.images.push_back(std::move(img));
			}
 			newTexture.data = nullptr;
			newTexture.dataSize=0;
			
			texturesToCompress.emplace(id, pct);
		}
	}
	else
	{
		newTexture.compression = avs::TextureCompression::UNCOMPRESSED;
		
		unsigned char* dataCopy = new unsigned char[newTexture.dataSize];
		memcpy(dataCopy, newTexture.data, newTexture.dataSize);
		newTexture.data = dataCopy;
		if(newTexture.dataSize > 1048576)
		{
			TELEPORT_WARN << "Texture \"" << newTexture.name << "\" was stored UNCOMPRESSED with a data size larger than 1MB! Size: " << newTexture.dataSize << "B(" << newTexture.dataSize / 1048576.0f << "MB).\n";
		}
	}

	textures[id] = ExtractedTexture{ path, lastModified, newTexture };
}

avs::uid GeometryStore::storeFont(_bstr_t ttf_path_utf8,_bstr_t relative_asset_path_utf8,std::time_t lastModified,int size)
{
	avs::Texture avsTexture;
	std::string cacheFontFilePath=relative_asset_path_utf8+".font";
	std::string cacheTextureFilePath=relative_asset_path_utf8+".png";
	avs::uid font_atlas_uid=GetOrGenerateUid(cacheFontFilePath);
	avs::uid font_texture_uid=GetOrGenerateUid(cacheTextureFilePath);
	ExtractedFontAtlas &fa=fontAtlases[font_atlas_uid];
	std::vector<int> sizes={size};
	Font::ExtractFont(fa.fontAtlas,ttf_path_utf8,(cachePath+"/"s+cacheTextureFilePath).c_str(),"ABCDEFGHIJKLMNOPQRSTUVWXYZ",avsTexture,sizes);
	std::filesystem::path p=std::string(ttf_path_utf8);
	saveResourceBinary(cacheFontFilePath,fa);
	storeTexture(font_texture_uid,"",relative_asset_path_utf8, std::time_t(), avsTexture,cachePath+"/"s+cacheTextureFilePath, true,	 true,true);
	fa.fontAtlas.font_texture_uid=font_texture_uid;
	//Font::Free(avsTexture);
	return font_atlas_uid;
}

avs::uid GeometryStore::storeTextCanvas( _bstr_t relative_asset_path, const InteropTextCanvas *interopTextCanvas)
{
	if(!interopTextCanvas||!interopTextCanvas->text)
		return 0;
	avs::uid canvas_uid=GetOrGenerateUid(avs::convertToByteString(relative_asset_path.GetBSTR()));
	teleport::TextCanvas &textCanvas=textCanvases[canvas_uid];

	textCanvas.text=avs::convertToByteString(interopTextCanvas->text);
	std::string cacheFontFilePath=avs::convertToByteString(interopTextCanvas->font)+".font";
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

void GeometryStore::storeShadowMap(avs::uid id, _bstr_t guid,_bstr_t path, std::time_t lastModified, avs::Texture& newShadowMap)
{
	shadowMaps[id] = ExtractedTexture{ path, lastModified, newShadowMap};
}

void GeometryStore::removeNode(avs::uid id)
{
	nodes.erase(id);
	lightNodes.erase(id);
}

void GeometryStore::updateNodeTransform(avs::uid id, avs::Transform& newLocalTransform, avs::Transform& newGlobalTransform)
{
	auto nodeIt = nodes.find(id);
	if(nodeIt == nodes.end())
		return;

	nodeIt->second.localTransform = newLocalTransform;
	nodeIt->second.globalTransform= newGlobalTransform;
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

void GeometryStore::compressNextTexture()
{
	//No textures to compress.
	if(texturesToCompress.size() == 0)
		return;
	auto compressionPair = texturesToCompress.begin();
	auto& foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());

	avs::Texture& newTexture = foundTexture->second.texture;
	PrecompressedTexture& compressionData = compressionPair->second;
	if (compressionData.textureCompression == avs::TextureCompression::BASIS_COMPRESSED)
	{
		basisu::basis_compressor_params basisCompressorParams; //Parameters for basis compressor.
		basisCompressorParams.m_source_images.clear();
		// Basis stores mip 0 in m_source_images, and subsequent mips in m_source_mipmap_images.
		// They MUST have equal sizes.
		if(compressionData.numMips<1)
		{
			TELEPORT_CERR<<"Bad mipcount "<<compressionData.numMips<<"\n";
			texturesToCompress.erase(texturesToCompress.begin());
			return;
		}
		size_t imagesPerMip=compressionData.images.size()/compressionData.numMips;
		if(imagesPerMip*compressionData.numMips!=compressionData.images.size())
		{
			TELEPORT_CERR<<"Bad image count "<<compressionData.images.size()<<" for "<<compressionData.numMips<<" mips.\n";
			texturesToCompress.erase(texturesToCompress.begin());
			return;
		}
		size_t n=0;
		int w=newTexture.width;
		int h=newTexture.height;
		//compressionData.numMips=std::min((size_t)2,compressionData.numMips);
		for(size_t m=0;m<compressionData.numMips;m++)
		{
			for(size_t i=0;i<imagesPerMip;i++)
			{
				if(m>0&&basisCompressorParams.m_source_mipmap_images.size()<imagesPerMip)
					basisCompressorParams.m_source_mipmap_images.push_back(basisu::vector<basisu::image>());
				basisu::image image(w, h);
				// TODO: This ONLY works for 8-bit rgba.
				basisu::color_rgba_vec& imageData = image.get_pixels();
				std::vector<uint8_t> &img=compressionData.images[n];
				if(img.size()>4*imageData.size())
				{
					TELEPORT_CERR<<"Image data size mismatch.\n";
					return;
				}
				if(img.size()<4*imageData.size())
				{
					TELEPORT_CERR<<"Image data size mismatch.\n";
				}
				memcpy(imageData.data(),img.data(),img.size());
				if(m==0)
					basisCompressorParams.m_source_images.push_back(std::move(image));
				else
					basisCompressorParams.m_source_mipmap_images[i].push_back(std::move(image));
				n++;
			}
			w=(w+1)/2;
			h=(h+1)/2;
		}
		// TODO: This doesn't work for mips>0. So can't flip textures from Unity for example.
		//basisCompressorParams.m_y_flip=true;
		basisCompressorParams.m_quality_level = compressionQuality;
		basisCompressorParams.m_compression_level = compressionStrength;

		basisCompressorParams.m_write_output_basis_files = true;
		basisCompressorParams.m_out_filename = compressionData.basisFilePath;
		basisCompressorParams.m_uastc = compressionData.highQualityUASTC;

		uint32_t num_threads = 32;
		if (compressionData.highQualityUASTC)
		{
			num_threads = 1;
			// Write this to a different filename, it's just for testing.
			auto ext_pos = basisCompressorParams.m_out_filename.find(".basis");
			basisCompressorParams.m_out_filename = basisCompressorParams.m_out_filename.substr(0, ext_pos) + "-dll.basis";
			return;

			// we want the equivalent of:
			// -uastc -uastc_rdo_m -no_multithreading -debug -stats -output_path "outputPath" "srcPng"
			basisCompressorParams.m_rdo_uastc_multithreading = false;
			basisCompressorParams.m_multithreading = false;
			//basisCompressorParams.m_ktx2_uastc_supercompression = basist::KTX2_SS_NONE;//= basist::KTX2_SS_ZSTANDARD;

			int uastc_level = std::clamp<int>(4, 0, 4);

			//static const uint32_t s_level_flags[5] = { basisu::cPackUASTCLevelFastest, basisu::cPackUASTCLevelFaster, basisu::cPackUASTCLevelDefault, basisu::cPackUASTCLevelSlower, basisu::cPackUASTCLevelVerySlow };

			//basisCompressorParams.m_pack_uastc_flags &= ~basisu::cPackUASTCLevelMask;
			//basisCompressorParams.m_pack_uastc_flags |= s_level_flags[uastc_level];

			//basisCompressorParams.m_rdo_uastc_dict_size = 32768;
			//basisCompressorParams.m_check_for_alpha=true;
			basisCompressorParams.m_debug = true;
			basisCompressorParams.m_status_output = true;
			basisCompressorParams.m_compute_stats = true;
			//basisCompressorParams.m_perceptual=true;
			//basisCompressorParams.m_validate=false;
			basisCompressorParams.m_mip_srgb = true;
			basisCompressorParams.m_quality_level = 128;
		}
		else
		{
			basisCompressorParams.m_mip_gen = compressionData.genMips;
			basisCompressorParams.m_mip_smallest_dimension = 4; // ???
		}
		basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexType2D;
		if(newTexture.cubemap)
		{
			basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexTypeCubemapArray;
		}
		if (!basisCompressorParams.m_pJob_pool)
		{
			basisCompressorParams.m_pJob_pool = new basisu::job_pool(num_threads);
		}
		basisu::basis_compressor basisCompressor;
		basisu::enable_debug_printf(true);

		bool ok = basisCompressor.init(basisCompressorParams);
		if(!ok)
		{
			basisu::basisu_encoder_init(false,false);
			ok = basisCompressor.init(basisCompressorParams);
		}
		if (ok)
		{
			basisu::basis_compressor::error_code result = basisCompressor.process();

			if (result == basisu::basis_compressor::error_code::cECSuccess)
			{
				basisu::uint8_vec basisTex = basisCompressor.get_output_basis_file();

			//	delete[] newTexture.data;

				newTexture.dataSize = basisCompressor.get_basis_file_size();
				newTexture.data = new unsigned char[newTexture.dataSize];
				memcpy(newTexture.data, basisTex.data(), newTexture.dataSize);
			}
			else
			{
				TELEPORT_CERR << "Failed to compress texture \"" << newTexture.name << "\"!\n";
			}
		}
		else
		{
			TELEPORT_CERR << "Failed to compress texture \"" << newTexture.name << "\"! Basis Universal compressor failed to initialise.\n";
		}
		delete basisCompressorParams.m_pJob_pool;
		basisCompressorParams.m_pJob_pool = nullptr;
	}
	else
	{
		// TODO: just store?
		TELEPORT_CERR << "Failed to compress texture \"" << newTexture.name << "\"!\n";
	}
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
	std::ofstream resourceFile(file_name.c_str(), std::ios::binary);
	resourceFile.unsetf(std::ios_base::skipws);
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
	// verify:
	{
		std::ifstream verifyFile(file_name.c_str(), std::ios::binary);
		verifyFile.unsetf(std::ios_base::skipws);
		ExtractedResource verifyResource;
		verifyFile>>verifyResource;
		verifyFile.close();
		if(!resource.Verify(verifyResource))
		{
			TELEPORT_CERR<<"File Verification failed for "<<file_name.c_str()<<"\n";
			teleport::DebugBreak();
			std::ifstream verifyFile(file_name.c_str(), std::ios::binary);
			verifyFile.unsetf(std::ios_base::skipws);
			ExtractedResource  verifyResource2;
			verifyFile>>verifyResource2;
			verifyFile.close();
			
			std::ofstream saveFile(file_name.c_str(), std::ios::binary);
			saveFile << resource;
			saveFile << "\n";
			return false;
		}
	}
	//Delete old file.
	if (oldFileExists)
		filesystem::remove(file_name + ".bak");
	return true;
}

template<typename ExtractedResource> bool GeometryStore::loadResourceBinary(const std::string file_name,const std::string &path_root, ExtractedResource &resource)
{
	std::ifstream resourceFile(file_name.c_str(), std::ios::binary);
	resourceFile.unsetf(std::ios_base::skipws);
	//auto write_time= std::filesystem::last_write_time(file_name);
	try
	{
		resourceFile >> newResource;
	}
	catch(...)
	{
		TELEPORT_CERR<<"Failed to load "<<file_name.c_str()<<"\n";
		return false;
	}
	return true;
}

template<typename ExtractedResource> bool GeometryStore::saveResource(const std::string file_name, const ExtractedResource& resource) const
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
	auto f=std::bind(&GeometryStore::UidToPath,this,std::placeholders::_1);
	resource_ofstream resourceFile(file_name.c_str(), f);
	try
	{
		resourceFile << resource;
		resourceFile << "\n";
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
	// verify:
	{
		resource_ifstream verifyFile(file_name.c_str(), std::bind(&GeometryStore::PathToUid,this,std::placeholders::_1));
		ExtractedResource  verifyResource;
		verifyFile>>verifyResource;
		verifyFile.close();
		if(!resource.Verify(verifyResource))
		{
			TELEPORT_CERR<<"File Verification failed for "<<file_name.c_str()<<"\n";
			teleport::DebugBreak();
			resource_ifstream verifyFile(file_name.c_str(), std::bind(&GeometryStore::PathToUid,this,std::placeholders::_1));
			ExtractedResource  verifyResource2;
			verifyFile>>verifyResource2;
			verifyFile.close();
			
			resource_ofstream saveFile(file_name.c_str(), f);
			saveFile << resource;
			saveFile << "\n";
			return false;
		}
	}
	//Delete old file.
	if (oldFileExists)
		filesystem::remove(file_name + ".bak");
	return true;
}


template<typename ExtractedResource> bool GeometryStore::saveResources(const std::string path, const std::map<avs::uid, ExtractedResource>& resourceMap) const
{
	const std::filesystem::path fspath{ path.c_str() };
	std::filesystem::create_directories(fspath);
	for(const auto& resourceData : resourceMap)
	{
		std::string file_name=(path+"/")+MakeResourceFilename(resourceData.second);
		if(!saveResource(file_name,resourceData.second))
			return false;
	}
	return true;
}


template<typename ExtractedResource> avs::uid GeometryStore::loadResource(const std::string file_name,const std::string &path_root,std::map<avs::uid, ExtractedResource>& resourceMap)
{
	resource_ifstream resourceFile(file_name.c_str(), std::bind(&GeometryStore::PathToUid,this,std::placeholders::_1));
	std::string p=StandardizePath(file_name,path_root);
	size_t ext_pos = p.find(ExtractedResource::fileExtension());
	if (ext_pos < p.length())
		p = p.substr(0, ext_pos);
	auto write_time= std::filesystem::last_write_time(file_name);
	// If there's a duplicate, use the newer file.
	// This guid might already exist!
	avs::uid newID=0;
	auto u=path_to_uid.find(p);
	if(u!=path_to_uid.end())
	{
		newID=u->second;
	}
	else
	{
		newID=avs::GenerateUid();
	}
	ExtractedResource& newResource = resourceMap[newID];
	try
	{
		resourceFile >> newResource;
		//TELEPORT_COUT<<"Loaded Resource "<<newResource.getName().c_str()<<" from file "<<file_name.c_str()<<"\n";
	}
	catch(...)
	{
		TELEPORT_CERR<<"Failed to load "<<file_name.c_str()<<"\n";
		return 0;
	}
	standardize_path(p);
	uid_to_path[newID]=p;
	path_to_uid[p]=newID;
	return newID;
}

template<typename ExtractedResource> void GeometryStore::loadResources(const std::string path, std::map<avs::uid, ExtractedResource>& resourceMap)
{
	//Load resources if the file exists.
	const std::filesystem::path fspath{ path.c_str() };
	std::filesystem::create_directories(fspath);
	std::string search_str=ExtractedResource::fileExtension();
	std::map<avs::uid, std::filesystem::file_time_type> timestamps;
	for (auto const& dir_entry : std::filesystem::recursive_directory_iterator{ fspath })
	{
		std::string file_name=dir_entry.path().string();
		if(file_name.find(search_str)<file_name.length())
		if(filesystem::exists(file_name))
		{
			loadResource(file_name,path,resourceMap);
		}
	}
}

avs::uid GeometryStore::PathToUid(std::string p) const
{
	p=StandardizePath(p,"");
	if(p.size()<2)
		return 0;
	auto i=path_to_uid.find(p);
	if(i==path_to_uid.end())
	{
		TELEPORT_INTERNAL_BREAK_ONCE("No uid for this path.");
		//TELEPORT_CERR<<"path "<<p.c_str()<<" not found.\n";
		return 0;
	}
	return i->second;
}

std::string GeometryStore::UidToPath(avs::uid u) const
{
	auto i=uid_to_path.find(u);
	if(i==uid_to_path.end())
	{
		TELEPORT_INTERNAL_BREAK_ONCE("No path for this uid.");
		return "";
		//throw std::runtime_error("No path for this uid.");
	}
	return i->second;
}
std::set<avs::uid> GeometryStore::GetClashingUids() const
{
	std::set<avs::uid> clash_uids;
	for(auto m:meshes)
	{
		std::set<avs::uid> mesh_uids;
		std::map<avs::Accessor::ComponentType,std::set<avs::uid>> component_uids;
		avs::AxesStandard axesStandard=m.first;
#if 0
		for(const auto &mesh:m.second)
		{
			for(const auto &mesh2:m.second)
			{
				for(const auto &a:mesh2.second.mesh.accessors)
				{
					if(mesh.first==a.first)
					{
						clash_uids.insert(mesh.first);
						TELEPORT_CERR<<"UID clash, mesh "<<mesh.first<<" "<<mesh.second.getName().c_str()
							<<" clashes with accessor in mesh "<<mesh2.first<<" "<<mesh2.second.getName().c_str()<<"\n";
					}
				}
			}
		}
#endif
	}
	return clash_uids;
}

bool GeometryStore::CheckForErrors() const
{
	auto clashes=GetClashingUids();
	if(clashes.size())
		return false;
	return true;
}

avs::uid GeometryStore::GetOrGenerateUid(const std::string &path)
{
	std::string p=path;
	if(p.size()<2)
		return 0;
	standardize_path(p);
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