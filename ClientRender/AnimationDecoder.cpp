#include "AnimationDecoder.h"

#include "Node.h"
#include "Transform.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/base/endianness.h"
#include "Platform/CrossPlatform/AxesStandard.h"

#define tinygltf teleport_tinygltf
#include "tiny_gltf.h"
#include "ozz/gltf2ozz.h"
#include <json.hpp>
#include <ozz/base/containers/set.h>
#include <algorithm>

using nlohmann::json;

using qt = platform::crossplatform::Quaternionf;
using namespace teleport;

using namespace clientrender;

//! VRM humanoid role for each glTF node that fills one, keyed by node index.
//!
//! Retargeting matches joints by name, which works only while a rig happens to name its
//! bones after the humanoid roles they play. That is a convention, not a contract: both
//! VRM and VRM-Animation state the mapping explicitly, as node indices, and a file is free
//! to call its nodes anything at all. Where the declaration exists it is authoritative and
//! the names are not consulted; where it does not, the caller falls back to the node name
//! as before.
//!
//! Three layouts are read, because the spec moved between versions:
//!   VRM 0.x         extensions.VRM.humanoid.humanBones          array of {bone, node}
//!   VRM 1.0         extensions.VRMC_vrm.humanoid.humanBones     object of role -> {node}
//!   VRM-Animation   extensions.VRMC_vrm_animation.humanoid.humanBones   ditto
//! The 1.0-beta animation layout carries inline sampler references and no node index; it
//! yields nothing here and falls back to names, which is all that layout offers.
static std::unordered_map<int, std::string> BuildVrmRoleMap(const tinygltf::Model &model)
{
	std::unordered_map<int, std::string> roles;
	auto readObjectForm = [&roles, &model](const tinygltf::Value &humanBones)
	{
		if (!humanBones.IsObject())
		{
			return;
		}
		for (const std::string &role : humanBones.Keys())
		{
			const tinygltf::Value &entry = humanBones.Get(role);
			if (!entry.IsObject() || !entry.Has("node"))
			{
				continue;
			}
			const int nodeIndex = entry.Get("node").GetNumberAsInt();
			if (nodeIndex >= 0 && nodeIndex < (int)model.nodes.size())
			{
				roles[nodeIndex] = role;
			}
		}
	};
	auto humanoidOf = [&model](const char *extension) -> const tinygltf::Value *
	{
		auto e = model.extensions.find(extension);
		if (e == model.extensions.end() || !e->second.IsObject() || !e->second.Has("humanoid"))
		{
			return nullptr;
		}
		const tinygltf::Value &humanoid = e->second.Get("humanoid");
		return humanoid.Has("humanBones") ? &humanoid : nullptr;
	};

	if (const tinygltf::Value *humanoid = humanoidOf("VRMC_vrm_animation"))
	{
		readObjectForm(humanoid->Get("humanBones"));
	}
	if (roles.empty())
	{
		if (const tinygltf::Value *humanoid = humanoidOf("VRMC_vrm"))
		{
			readObjectForm(humanoid->Get("humanBones"));
		}
	}
	if (roles.empty())
	{
		// VRM 0.x: an array of {bone, node} rather than an object keyed by role.
		if (const tinygltf::Value *humanoid = humanoidOf("VRM"))
		{
			const tinygltf::Value &humanBones = humanoid->Get("humanBones");
			if (humanBones.IsArray())
			{
				for (size_t i = 0; i < humanBones.ArrayLen(); i++)
				{
					const tinygltf::Value &entry = humanBones.Get((int)i);
					if (!entry.IsObject() || !entry.Has("bone") || !entry.Has("node"))
					{
						continue;
					}
					const int nodeIndex = entry.Get("node").GetNumberAsInt();
					if (nodeIndex >= 0 && nodeIndex < (int)model.nodes.size())
					{
						roles[nodeIndex] = entry.Get("bone").Get<std::string>();
					}
				}
			}
		}
	}
	if (roles.size())
	{
		TELEPORT_INTERNAL_COUT(Default, "VRM humanoid: {} bones named by declared role rather than by node name.", roles.size());
	}
	return roles;
}

//! The name a joint should be known by: its declared humanoid role where it has one, and
//! its glTF node name otherwise.
static std::string EffectiveNodeName(const tinygltf::Model &model, const std::unordered_map<int, std::string> &roles, int nodeIndex)
{
	auto r = roles.find(nodeIndex);
	if (r != roles.end())
	{
		return r->second;
	}
	return (nodeIndex >= 0 && nodeIndex < (int)model.nodes.size()) ? model.nodes[nodeIndex].name : std::string();
}

// Returns all skins belonging to a given gltf scene
ozz::vector<tinygltf::Skin> GetSkinsForScene(const tinygltf::Scene &_scene, const tinygltf::Model &model)
{
	ozz::set<int> open;
	ozz::set<int> found;

	for (int nodeIndex : _scene.nodes)
	{
		open.insert(nodeIndex);
	}

	while (!open.empty())
	{
		int nodeIndex = *open.begin();
		found.insert(nodeIndex);
		open.erase(nodeIndex);

		auto &node = model.nodes[nodeIndex];
		for (int childIndex : node.children)
		{
			open.insert(childIndex);
		}
	}

	ozz::vector<tinygltf::Skin> skins;
	for (const tinygltf::Skin &skin : model.skins)
	{
		if (!skin.joints.empty() && found.find(skin.joints[0]) != found.end())
		{
			skins.push_back(skin);
		}
	}

	return skins;
}
// Find all unique root joints of skeletons used by given skins and add them
// to `roots`
void FindSkinRootJointIndices(const ozz::vector<tinygltf::Skin> &skins, const tinygltf::Model &model, ozz::vector<int> &roots)
{
	static constexpr int no_parent = -1;
	static constexpr int visited   = -2;
	ozz::vector<int>	 parents(model.nodes.size(), no_parent);
	for (int node = 0; node < static_cast<int>(model.nodes.size()); node++)
	{
		for (int child : model.nodes[node].children)
		{
			parents[child] = node;
		}
	}

	for (const tinygltf::Skin &skin : skins)
	{
		if (skin.joints.empty())
		{
			continue;
		}

		if (skin.skeleton != -1)
		{
			parents[skin.skeleton] = visited;
			roots.push_back(skin.skeleton);
			continue;
		}

		int root = skin.joints[0];
		while (root != visited && parents[root] != no_parent)
		{
			root = parents[root];
		}
		if (root != visited)
		{
			roots.push_back(root);
		}
	}
}
static std::vector<std::string> name_order = {"hips",		   "spine",			"chest",		"upperChest",	"neck",			 "head",
											  "leftShoulder",  "leftUpperArm",	"leftLowerArm", "leftHand",		"rightShoulder", "rightUpperArm",
											  "rightLowerArm", "rightHand",		"leftUpperLeg", "leftLowerLeg", "leftFoot",		 "leftToes",
											  "rightUpperLeg", "rightLowerLeg", "rightFoot",	"rightToes"};


static ozz::math::Float3 ConvertPosition(platform::crossplatform::AxesStandard sourceAxesStandard,
											platform::crossplatform::AxesStandard targetAxesStandard,
											ozz::math::Float3 v)
{
	vec3 p=platform::crossplatform::ConvertPosition(sourceAxesStandard,targetAxesStandard,*((vec3*)&v));
	return *((ozz::math::Float3*)&p);
}

static ozz::math::Float4 ConvertRotation(platform::crossplatform::AxesStandard sourceAxesStandard,
											platform::crossplatform::AxesStandard targetAxesStandard,
											ozz::math::Float4 v)
{
	platform::crossplatform::Quaternionf q=platform::crossplatform::ConvertRotation(sourceAxesStandard,targetAxesStandard,*((platform::crossplatform::Quaternionf*)&v));
	return *((ozz::math::Float4*)&q);
}

static ozz::math::Quaternion ConvertRotation(platform::crossplatform::AxesStandard sourceAxesStandard,
											platform::crossplatform::AxesStandard targetAxesStandard,
											ozz::math::Quaternion v)
{
	platform::crossplatform::Quaternionf q=platform::crossplatform::ConvertRotation(sourceAxesStandard,targetAxesStandard,*((platform::crossplatform::Quaternionf*)&v));
	return *((ozz::math::Quaternion*)&q);
}

static ozz::math::Float3 ConvertScale(platform::crossplatform::AxesStandard sourceAxesStandard,
										platform::crossplatform::AxesStandard targetAxesStandard,
										ozz::math::Float3 v)
{
	vec3 p=platform::crossplatform::ConvertScale(sourceAxesStandard,targetAxesStandard,*((vec3*)&v));
	return *((ozz::math::Float3*)&p);
}

// Recursively import a node's children
bool ImportNode(int												 nodeIndex,
				const tinygltf::Model							 &model,
				const std::unordered_map<int, std::string>		 &roles,
				ozz::animation::offline::RawSkeleton::Joint		 *_joint,
				platform::crossplatform::AxesStandard			sourceAxesStandard,
				platform::crossplatform::AxesStandard			targetAxesStandard)
{
	const tinygltf::Node &gltfNode = model.nodes[nodeIndex];
	// Named by declared humanoid role where there is one, so that retargeting matches on
	// the role a bone plays rather than on what its author happened to call it.
	_joint->name = EffectiveNodeName(model, roles, nodeIndex).c_str();

	// Fills transform.
	if (!CreateNodeTransform(gltfNode, &_joint->transform))
	{
		return false;
	}
	_joint->transform.translation	= ConvertPosition(sourceAxesStandard,targetAxesStandard,_joint->transform.translation);
	_joint->transform.rotation		= ConvertRotation(sourceAxesStandard,targetAxesStandard,_joint->transform.rotation);
	_joint->transform.scale			= ConvertScale(sourceAxesStandard,targetAxesStandard,_joint->transform.scale);


	_joint->children.resize(gltfNode.children.size());
	// Fills each child information.
	auto sorted_children = gltfNode.children;
	
	for (size_t i = 0; i < sorted_children.size(); ++i)
	{
		ozz::animation::offline::RawSkeleton::Joint &child_joint = _joint->children[i];

		if (!ImportNode(sorted_children[i], model, roles, &child_joint, sourceAxesStandard, targetAxesStandard))
		{
			return false;
		}
	}

	return true;
}

bool ImportSkeleton(ozz::animation::offline::RawSkeleton			 &raw_skeleton,
					const tinygltf::Model							 &model,
					platform::crossplatform::AxesStandard			sourceAxesStandard,
					platform::crossplatform::AxesStandard			targetAxesStandard)
{
	if (model.scenes.empty())
	{
		TELEPORT_WARN("No scenes found.");
		return false;
	}

	// If no default scene has been set then take the first one spec does not
	// disallow gltfs without a default scene but it makes more sense to keep
	// going instead of throwing an error here
	int defaultScene = model.defaultScene;
	if (defaultScene == -1)
	{
		defaultScene = 0;
	}

	const tinygltf::Scene &scene = model.scenes[defaultScene];
	TELEPORT_INTERNAL_COUT(Default, "Importing from default scene #{} with name {}", defaultScene, scene.name);

	if (scene.nodes.empty())
	{
		TELEPORT_INTERNAL_COUT(Default, "Scene has no node.");
		return false;
	}

	// Get all the skins belonging to this scene
	ozz::vector<int>			roots;
	ozz::vector<tinygltf::Skin> skins = GetSkinsForScene(scene, model);
	if (skins.empty())
	{
		TELEPORT_INTERNAL_COUT(Default, "No skin exists in the scene, the whole scene graph "
					 "will be considered as a skeleton.");
		// Uses all scene nodes.
		for (auto &node : scene.nodes)
		{
			roots.push_back(node);
		}
	}
	else
	{
		if (skins.size() > 1)
		{
			TELEPORT_INTERNAL_COUT(Default, "Multiple skins exist in the scene, they will all "
						 "be exported to a single skeleton.");
		}

		// Uses all skins roots.
		FindSkinRootJointIndices(skins, model, roots);
	}

	// Remove nodes listed multiple times.
	std::sort(roots.begin(), roots.end());
	roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

	// Traverses the scene graph and record all joints starting from the roots.
	const std::unordered_map<int, std::string> roles = BuildVrmRoleMap(model);
	raw_skeleton.roots.resize(roots.size());
	for (size_t i = 0; i < roots.size(); ++i)
	{
		ozz::animation::offline::RawSkeleton::Joint &root_joint = raw_skeleton.roots[i];
		if (!ImportNode(roots[i], model, roles, &root_joint, sourceAxesStandard, targetAxesStandard))
		{
			return false;
		}
	}

	if (!raw_skeleton.Validate())
	{
		TELEPORT_INTERNAL_COUT(Default, "Output skeleton failed validation. This is likely an implementation issue.");
		return false;
	}
	return true;
}

bool SampleAnimationChannel(const tinygltf::Model							  &model,
							const tinygltf::AnimationSampler				  &_sampler,
							const std::string								  &_target_path,
							float											   _sampling_rate,
							float											  *_duration,
							ozz::animation::offline::RawAnimation::JointTrack *_track,
							platform::crossplatform::AxesStandard sourceAxesStandard,
							platform::crossplatform::AxesStandard targetAxesStandard)
{
	// Validate interpolation type.
	if (_sampler.interpolation.empty())
	{
		TELEPORT_INTERNAL_COUT(Default, "Invalid sampler interpolation.");
		return false;
	}

	if (_sampler.input < 0 || _sampler.input >= static_cast<int>(model.accessors.size()))
	{
		TELEPORT_INTERNAL_COUT(Default, "Animation sampler input accessor index out of range.");
		return false;
	}
	auto &input = model.accessors[_sampler.input];

	const ozz::span<const float> timestamps = BufferView<float>(model, input);

	// The max[0] property of the input accessor is the animation duration
	// this is required to be present by the spec:
	// "Animation Sampler's input accessor must have min and max properties
	// defined."
	// tinygltf does not enforce this, so files from some exporters (e.g.
	// VRoid .vrma) can leave maxValues empty; in that case derive the
	// duration from the largest timestamp in the accessor data itself
	// rather than rejecting the whole animation.
	float duration = 0.0f;
	if (input.maxValues.size() == 1)
	{
		duration = static_cast<float>(input.maxValues[0]);
	}
	else if (!timestamps.empty())
	{
		TELEPORT_INTERNAL_COUT(Default, "Animation sampler input accessor is missing its max property. Deriving duration from timestamp data.");
		duration = *std::max_element(timestamps.begin(), timestamps.end());
	}

	// If this channel's duration is larger than the animation's duration
	// then increase the animation duration to match.
	if (duration > *_duration)
	{
		*_duration = duration;
	}

	if (input.type != TINYGLTF_TYPE_SCALAR)
	{
		TELEPORT_INTERNAL_COUT(Default, "Animation sampler input accessor has an unexpected type.");
		return false;
	}
	if (_sampler.output < 0 || _sampler.output >= static_cast<int>(model.accessors.size()))
	{
		TELEPORT_INTERNAL_COUT(Default, "Animation sampler output accessor index out of range.");
		return false;
	}
	auto &_output = model.accessors[_sampler.output];
	if (_output.type != TINYGLTF_TYPE_VEC3 && _output.type != TINYGLTF_TYPE_VEC4)
	{
		TELEPORT_INTERNAL_COUT(Default, "Animation sampler output accessor has an unexpected type.");
		return false;
	}

	if (timestamps.empty())
	{
		return true;
	}

	// Builds keyframes.
	bool valid = false;
	if (_target_path == "translation")
	{
		valid = SampleChannel(model, _sampler.interpolation, _output, timestamps, _sampling_rate, duration, &_track->translations);
		for(auto &t:_track->translations)
		{
			t.value=ConvertPosition(sourceAxesStandard,targetAxesStandard,t.value);
		}
	}
	else if (_target_path == "rotation")
	{
		valid = SampleChannel(model, _sampler.interpolation, _output, timestamps, _sampling_rate, duration, &_track->rotations);
		if (valid)
		{
			// Normalize quaternions.
			for (auto &key : _track->rotations)
			{
				key.value = ozz::math::Normalize(key.value);
				key.value = ConvertRotation(sourceAxesStandard,targetAxesStandard,key.value);
			}
		}
	}
	else if (_target_path == "scale")
	{
		valid = SampleChannel(model, _sampler.interpolation, _output, timestamps, _sampling_rate, duration, &_track->scales);
		for (auto &s : _track->scales)
			s.value=ConvertScale(sourceAxesStandard,targetAxesStandard,s.value);
	}
	else
	{
		assert(false && "Invalid target path");
	}

	return valid;
}

const tinygltf::Node *FindNodeByName(const tinygltf::Model &model, const std::string &_name)
{
	for (const tinygltf::Node &node : model.nodes)
	{
		if (node.name == _name)
		{
			return &node;
		}
	}

	return nullptr;
}
bool ImportAnimations(const tinygltf::Model					&model,
					  const ozz::animation::Skeleton		&skeleton,
					  float									 _sampling_rate,
					  ozz::animation::offline::RawAnimation *_animation,
					  platform::crossplatform::AxesStandard sourceAxesStandard,
					  platform::crossplatform::AxesStandard targetAxesStandard)
{
	if (_sampling_rate == 0.0f)
	{
		_sampling_rate				 = 30.0f;

		static bool samplingRateWarn = false;
		if (!samplingRateWarn)
		{
			TELEPORT_INTERNAL_COUT(Default, "The animation sampling rate is set to 0 "
						 "(automatic) but glTF does not carry scene frame "
						 "rate information. Assuming a sampling rate of {} Hz",
						 _sampling_rate);

			samplingRateWarn = true;
		}
	}

	// Find the corresponding gltf animation
	for (auto gltf_animation : model.animations)
	{
		_animation->name	 = gltf_animation.name.c_str();

		// Animation duration is determined during sampling from the duration of the
		// longest channel
		_animation->duration = 0.0f;

		const int num_joints = skeleton.num_joints();
		_animation->tracks.resize(num_joints);

		// gltf stores animations by splitting them in channels
		// where each channel targets a node's property i.e. translation, rotation
		// or scale. ozz expects animations to be stored per joint so we create a
		// map where we record the associated channels for each joint
		ozz::cstring_map<std::vector<const tinygltf::AnimationChannel *>> channels_per_joint;
		// Keyed by the same effective name ImportSkeleton gave the joints, so a rig whose
		// nodes are not named after their humanoid roles still lines up. cstring_map keys on
		// the pointer, so these have to outlive the map - hence one stable array of them
		// rather than a temporary per lookup.
		const std::unordered_map<int, std::string> roles = BuildVrmRoleMap(model);
		std::vector<std::string>				   effectiveNames(model.nodes.size());
		for (size_t n = 0; n < model.nodes.size(); n++)
		{
			effectiveNames[n] = EffectiveNodeName(model, roles, (int)n);
		}

		// std::cout << "Animation: " << gltf_animation.name << std::endl;
		for (const tinygltf::AnimationChannel &channel : gltf_animation.channels)
		{
			// Reject if no node is targeted.
			if (channel.target_node < 0 || channel.target_node >= model.nodes.size())
			{
				continue;
			}

			// What node?
			const auto &node  = model.nodes[channel.target_node];
			// std::cout << "    " << node.name << std::endl;
			//  Reject if path isn't about skeleton animation.
			bool valid_target = false;
			for (const char *path : {"translation", "rotation", "scale"})
			{
				valid_target |= channel.target_path == path;
			}
			if (!valid_target)
			{
				continue;
			}

			channels_per_joint[effectiveNames[channel.target_node].c_str()].push_back(&channel);
		}

		// For each joint get all its associated channels, sample them and record
		// the samples in the joint track
		const auto &joint_names = skeleton.joint_names();
		std::string joints;
		for (int i = 0; i < num_joints; i++)
		{
			auto	   &channels = channels_per_joint[joint_names[i]];
			auto	   &track	 = _animation->tracks[i];
			std::string j		 = joint_names[i];
			joints += j + " ";
			// if(j=="leftLowerArm")
			for (auto &channel : channels)
			{
				if (channel->sampler < 0 || channel->sampler >= static_cast<int>(gltf_animation.samplers.size()))
				{
					continue;
				}
				auto &sampler = gltf_animation.samplers[channel->sampler];
				if (!SampleAnimationChannel(model, sampler, channel->target_path, _sampling_rate, &_animation->duration, &track, sourceAxesStandard, targetAxesStandard))
				{
					continue;
				}
			}

			// By effective name, matching how the joint was named: a role-named joint has no
			// node of that name to find, and its rest pose would silently go unpadded.
			const tinygltf::Node *node = nullptr;
			for (size_t n = 0; n < effectiveNames.size(); n++)
			{
				if (effectiveNames[n] == joint_names[i])
				{
					node = &model.nodes[n];
					break;
				}
			}
			if (node == nullptr)
			{
				continue;
			}

			// Pads the rest pose transform for any joints which do not have an
			// associated channel for this animation
			if (track.translations.empty())
			{
				auto trans = CreateTranslationRestPoseKey(*node);
				trans.value = ConvertPosition(sourceAxesStandard,targetAxesStandard,trans.value);
				track.translations.push_back(trans);
			}
			if (track.rotations.empty())
			{
				auto rot = CreateRotationRestPoseKey(*node);
				rot.value = ConvertRotation(sourceAxesStandard,targetAxesStandard,rot.value);
				track.rotations.push_back(rot);
			}
			if (track.scales.empty())
			{
				auto sca = CreateScaleRestPoseKey(*node);
				sca.value = ConvertScale(sourceAxesStandard,targetAxesStandard,sca.value);
				track.scales.push_back(sca);
			}
		}
		// std::cout<<joints<<"\n";

		TELEPORT_INTERNAL_COUT(Default, "Processed animation '{}' (tracks: {}, duration: {} s).", _animation->name, _animation->tracks.size(), _animation->duration);

		if (!_animation->Validate())
		{
			TELEPORT_INTERNAL_COUT(Default, "Animation '{}' failed validation.", _animation->name);
			return false;
		}
	}
	return true;
}

bool Animation::LoadFromGlb(const uint8_t *data, size_t size, avs::AxesStandard sourceAxesStandard,
					avs::AxesStandard			targetAxesStandard)
{
	raw_skeleton  = ozz::make_unique<ozz::animation::offline::RawSkeleton>();
	raw_animation = ozz::make_unique<ozz::animation::offline::RawAnimation>();
	tinygltf::TinyGLTF loader;
	auto image_loader = [](tinygltf::Image *, const int, std::string *, std::string *, int, int, const unsigned char *, int, void *) { return true; };
	loader.SetImageLoader(image_loader, NULL);
	tinygltf::Model model;
	std::string		err;
	std::string		warn;
	if (!loader.LoadBinaryFromMemory(&model, &err, &warn, data, static_cast<unsigned int>(size), ""))
	{
		TELEPORT_INTERNAL_COUT(Default, "Failed to parse animation glb: {}", err);
		return false;
	}
	json config;
	//! Mapping from node names to the initial poses.
	if (!ImportSkeleton(*raw_skeleton, model, (platform::crossplatform::AxesStandard)sourceAxesStandard,  (platform::crossplatform::AxesStandard)targetAxesStandard))
	{
		return false;
	}
	ozz::animation::offline::SkeletonBuilder  skeletonBuilder;
	ozz::unique_ptr<ozz::animation::Skeleton> skeleton = skeletonBuilder(*raw_skeleton);
	if (!ImportAnimations(model, *skeleton, 0.0f, &(*raw_animation),  (platform::crossplatform::AxesStandard)sourceAxesStandard,  (platform::crossplatform::AxesStandard)targetAxesStandard))
	{
		TELEPORT_INTERNAL_COUT(Default, "Failed to import animations from glb.");
		return false;
	}
	duration=raw_animation->duration;
	return true;
}
