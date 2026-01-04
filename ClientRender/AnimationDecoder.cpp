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

using nlohmann::json;

using qt = platform::crossplatform::Quaternionf;
using namespace teleport;

using namespace clientrender;

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
bool ImportNode(const tinygltf::Node							 &gltfNode,
				const tinygltf::Model							 &model,
				ozz::animation::offline::RawSkeleton::Joint		 *_joint,
				platform::crossplatform::AxesStandard			sourceAxesStandard,
				platform::crossplatform::AxesStandard			targetAxesStandard)
{
	// Names joint.1
	_joint->name = gltfNode.name.c_str();

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
		const tinygltf::Node						&child_node	 = model.nodes[sorted_children[i]];
		ozz::animation::offline::RawSkeleton::Joint &child_joint = _joint->children[i];

		if (!ImportNode(child_node, model, &child_joint, sourceAxesStandard, targetAxesStandard))
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
	TELEPORT_LOG("Importing from default scene #{} with name {}", defaultScene, scene.name);

	if (scene.nodes.empty())
	{
		TELEPORT_LOG("Scene has no node.");
		return false;
	}

	// Get all the skins belonging to this scene
	ozz::vector<int>			roots;
	ozz::vector<tinygltf::Skin> skins = GetSkinsForScene(scene, model);
	if (skins.empty())
	{
		TELEPORT_LOG("No skin exists in the scene, the whole scene graph "
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
			TELEPORT_LOG("Multiple skins exist in the scene, they will all "
						 "be exported to a single skeleton.");
		}

		// Uses all skins roots.
		FindSkinRootJointIndices(skins, model, roots);
	}

	// Remove nodes listed multiple times.
	std::sort(roots.begin(), roots.end());
	roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

	// Traverses the scene graph and record all joints starting from the roots.
	raw_skeleton.roots.resize(roots.size());
	for (size_t i = 0; i < roots.size(); ++i)
	{
		const tinygltf::Node						&root_node	= model.nodes[roots[i]];
		ozz::animation::offline::RawSkeleton::Joint &root_joint = raw_skeleton.roots[i];
		if (!ImportNode(root_node, model, &root_joint, sourceAxesStandard, targetAxesStandard))
		{
			return false;
		}
	}

	if (!raw_skeleton.Validate())
	{
		TELEPORT_LOG("Output skeleton failed validation. This is likely an implementation issue.");
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
		TELEPORT_LOG("Invalid sampler interpolation.");
		return false;
	}

	auto &input = model.accessors[_sampler.input];
	assert(input.maxValues.size() == 1);

	// The max[0] property of the input accessor is the animation duration
	// this is required to be present by the spec:
	// "Animation Sampler's input accessor must have min and max properties
	// defined."
	const float duration = static_cast<float>(input.maxValues[0]);

	// If this channel's duration is larger than the animation's duration
	// then increase the animation duration to match.
	if (duration > *_duration)
	{
		*_duration = duration;
	}

	assert(input.type == TINYGLTF_TYPE_SCALAR);
	auto &_output = model.accessors[_sampler.output];
	assert(_output.type == TINYGLTF_TYPE_VEC3 || _output.type == TINYGLTF_TYPE_VEC4);

	const ozz::span<const float> timestamps = BufferView<float>(model, input);
	if (timestamps.empty())
	{
		return true;
	}

	// Builds keyframes.
	bool valid = false;
	if (_target_path == "translation")
	{
		// TODO: Restore translation
		//valid = SampleChannel(model, _sampler.interpolation, _output, timestamps, _sampling_rate, duration, &_track->translations);
		//for(auto &t:_track->translations)
		//{
		//	t.value=ConvertPosition(sourceAxesStandard,targetAxesStandard,t.value);
		//}
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
			TELEPORT_LOG("The animation sampling rate is set to 0 "
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

			const tinygltf::Node &target_node = model.nodes[channel.target_node];
			channels_per_joint[target_node.name.c_str()].push_back(&channel);
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
				auto &sampler = gltf_animation.samplers[channel->sampler];
				if (!SampleAnimationChannel(model, sampler, channel->target_path, _sampling_rate, &_animation->duration, &track, sourceAxesStandard, targetAxesStandard))
				{
					continue;
				}
			}

			const tinygltf::Node *node = FindNodeByName(model, joint_names[i]);
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

		TELEPORT_LOG("Processed animation '{}' (tracks: {}, duration: {} s).", _animation->name, _animation->tracks.size(), _animation->duration);

		if (!_animation->Validate())
		{
			TELEPORT_LOG("Animation '{}' failed validation.", _animation->name);
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
	loader.LoadBinaryFromMemory(&model, &err, &warn, data, static_cast<unsigned int>(size), "");
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
		return false;
	}
	duration=raw_animation->duration;
	return true;
}
