#include "AnimationDecoder.h"

#include "Node.h"
#include "Transform.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/base/endianness.h"

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
static std::vector<std::string> name_order={
    "hips",
    "spine",
    "chest",
    "upperChest",
    "neck",
    "head",
    "leftShoulder",
    "leftUpperArm",
    "leftLowerArm",
    "leftHand",
    "rightShoulder",
    "rightUpperArm",
    "rightLowerArm",
    "rightHand",
    "leftUpperLeg",
    "leftLowerLeg",
    "leftFoot",
    "leftToes",
    "rightUpperLeg",
    "rightLowerLeg",
    "rightFoot",
    "rightToes"};

// Recursively import a node's children
bool ImportNode(const tinygltf::Node &_node, const tinygltf::Model &model, ozz::animation::offline::RawSkeleton::Joint *_joint, std::map<std::string, teleport::core::PoseScale> &restPoses)
{
	// Names joint.
	_joint->name = _node.name.c_str();

	// Fills transform.
	if (!CreateNodeTransform(_node, &_joint->transform))
	{
		return false;
	}
	_joint->children.resize(_node.children.size());
	auto &restPose		 = restPoses[_node.name];
	restPose.position	 = {_joint->transform.translation.x, _joint->transform.translation.y, _joint->transform.translation.z};
	restPose.orientation = {_joint->transform.rotation.x, _joint->transform.rotation.y, _joint->transform.rotation.z, _joint->transform.rotation.w};
	restPose.scale		 = {_joint->transform.scale.x, _joint->transform.scale.y, _joint->transform.scale.z};

	// Fills each child information.
	auto sorted_children = _node.children;
	std::sort(sorted_children.begin(),sorted_children.end(),[&model](int a,int b)
	{
		std::string nameA=model.nodes[a].name;
		std::string nameB=model.nodes[b].name;
		int A=(int)(std::find(name_order.begin(),name_order.end(),nameA)-name_order.begin());
		int B=(int)(std::find(name_order.begin(),name_order.end(),nameB)-name_order.begin());
		return A<B;
	});
	for (size_t i = 0; i < sorted_children.size(); ++i)
	{
		const tinygltf::Node						&child_node	 = model.nodes[sorted_children[i]];
		ozz::animation::offline::RawSkeleton::Joint &child_joint = _joint->children[i];

		if (!ImportNode(child_node, model, &child_joint, restPoses))
		{
			return false;
		}
	}

	return true;
}

bool ImportSkeleton(ozz::animation::offline::RawSkeleton &raw_skeleton, ozz::unique_ptr<ozz::animation::Skeleton> &sk, const tinygltf::Model &model,std::map<std::string, teleport::core::PoseScale> &restPoses)
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
	TELEPORT_LOG("Importing from default scene #{} with name {}" ,defaultScene, scene.name);

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
						   "will be considered as a skeleton."
						);
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
		if (!ImportNode(root_node, model, &root_joint, restPoses))
		{
			return false;
		}
	}

	if (!raw_skeleton.Validate())
	{
		TELEPORT_LOG("Output skeleton failed validation. This is likely an implementation issue.");
		return false;
	}

	// Non unique joint names are not supported.
	// if (!(ozz::animation::offline::ValidateJointNamesUniqueness(raw_skeleton))) {
	// Log Err is done by the validation function.
	//  return false;
	//}

	// Needs to be done before opening the output file, so that if it fails then
	// there's no invalid file outputted.
	// Builds runtime skeleton.
	TELEPORT_LOG("Builds runtime skeleton.");
	ozz::animation::offline::SkeletonBuilder builder;
	sk = builder(raw_skeleton);
	if (!sk)
	{
		TELEPORT_LOG("Failed to build runtime skeleton.");
		return false;
	}
	return true;
}

bool SampleAnimationChannel(const tinygltf::Model							  &model,
							const tinygltf::AnimationSampler				  &_sampler,
							const std::string								  &_target_path,
							float											   _sampling_rate,
							float											  *_duration,
							ozz::animation::offline::RawAnimation::JointTrack *_track)
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
			}
		}
	}
	else if (_target_path == "scale")
	{
		valid = SampleChannel(model, _sampler.interpolation, _output, timestamps, _sampling_rate, duration, &_track->scales);
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
					  ozz::animation::offline::RawAnimation *_animation)
{
	if (_sampling_rate == 0.0f)
	{
		_sampling_rate				 = 30.0f;

		static bool samplingRateWarn = false;
		if (!samplingRateWarn)
		{
			TELEPORT_LOG( "The animation sampling rate is set to 0 "
								"(automatic) but glTF does not carry scene frame "
								"rate information. Assuming a sampling rate of {} Hz", _sampling_rate);

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

		std::cout << "Animation: " << gltf_animation.name << std::endl;
		for (const tinygltf::AnimationChannel &channel : gltf_animation.channels)
		{
			// Reject if no node is targeted.
			if (channel.target_node < 0 || channel.target_node >= model.nodes.size())
			{
				continue;
			}

			// What node?
			const auto &node=model.nodes[channel.target_node];
			std::cout << "    " << node.name << std::endl;
			// Reject if path isn't about skeleton animation.
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
			auto &channels = channels_per_joint[joint_names[i]];
			auto &track	   = _animation->tracks[i];
			std::string j=joint_names[i];
			joints+=j+" ";
			//if(j=="leftLowerArm")
			for (auto &channel : channels)
			{
				auto &sampler = gltf_animation.samplers[channel->sampler];
				if (!SampleAnimationChannel(model, sampler, channel->target_path, _sampling_rate, &_animation->duration, &track))
				{
					continue;
				}
			}

			const tinygltf::Node *node = FindNodeByName(model, joint_names[i]);
			if(node == nullptr)
			{
				continue;
			}

			// Pads the rest pose transform for any joints which do not have an
			// associated channel for this animation
			if (track.translations.empty())
			{
				track.translations.push_back(CreateTranslationRestPoseKey(*node));
			}
			if (track.rotations.empty())
			{
				track.rotations.push_back(CreateRotationRestPoseKey(*node));
			}
			if (track.scales.empty())
			{
				track.scales.push_back(CreateScaleRestPoseKey(*node));
			}
		}
		std::cout<<joints<<"\n";

		TELEPORT_LOG( "Processed animation '{}' (tracks: {}, duration: {} s).", _animation->name, _animation->tracks.size(), _animation->duration);

		if (!_animation->Validate())
		{
			TELEPORT_LOG("Animation '{}' failed validation.", _animation->name);
			return false;
		}
	}
	return true;
}

bool Animation::LoadFromGlb(const uint8_t *data, size_t size)
{
	raw_skeleton=ozz::make_unique<ozz::animation::offline::RawSkeleton>();
	raw_animation=ozz::make_unique<ozz::animation::offline::RawAnimation>( );
	tinygltf::TinyGLTF loader;
	auto image_loader = [](tinygltf::Image *, const int, std::string *, std::string *, int, int, const unsigned char *, int, void *) { return true; };
	loader.SetImageLoader(image_loader, NULL);
	tinygltf::Model model;
	std::string	err;
	std::string	warn;
	loader.LoadBinaryFromMemory(&model, &err, &warn, data, static_cast<unsigned int>(size), "");
	json config;
	if (!ImportSkeleton(*raw_skeleton, ozz_skeleton, model, restPoses))
		return false;
	ozz::animation::offline::SkeletonBuilder skeletonBuilder;
	ozz::unique_ptr<ozz::animation::Skeleton>	 skeleton = skeletonBuilder(*raw_skeleton);
	if (!ImportAnimations(model, *skeleton, 0.0f, &(*raw_animation)))
		return false;
	//ozz::animation::offline::AnimationBuilder animationBuilder;
	//ozz::animation::Animation ozz_animation = animationBuilder(*(raw_animation.get()));
	return true;
}
