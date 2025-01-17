#include <windows.h>
#include <string>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <queue>
#include <cmath>
#include <sstream>
#include "json.hpp"
#include "SceneObjectModel.h"

#pragma optimize("", off)

using nlohmann::json;

const std::string &SOMNode::getNodeName() const
{
	return nodeName;
}

std::shared_ptr<SOMNode> SOMNode::appendChild(std::shared_ptr<SOMNode> child)
{
	child->parentNode = shared_from_this();
	childNodes.push_back(child);
	return child;
}

const std::vector<std::shared_ptr<SOMNode>> &SOMNode::getChildNodes() const
{
	return childNodes;
}

void SOMNode::setPose(const Pose &newPose)
{
	localPose = newPose;
	markTransformDirty();
	notifyObservers([this](auto &obs)
					{ obs->onNodeMoved(shared_from_this()); });
}


bool SOMNode::intersectRay(const Ray &ray, double &distance) const
{
	// Simple sphere intersection test - replace with actual geometry
	double radius = 1.0; // Default collision sphere
	Vector3 toSphere = Vector3(
		worldPose.position.x - ray.origin.x,
		worldPose.position.y - ray.origin.y,
		worldPose.position.z - ray.origin.z);

	double b = 2.0 * (ray.direction.x * toSphere.x +
					  ray.direction.y * toSphere.y +
					  ray.direction.z * toSphere.z);
	double c = toSphere.x * toSphere.x +
			   toSphere.y * toSphere.y +
			   toSphere.z * toSphere.z -
			   radius * radius;

	double discriminant = b * b - 4.0 * c;
	if (discriminant < 0)
	{
		return false;
	}

	distance = (-b - std::sqrt(discriminant)) * 0.5;
	return distance >= 0;
}

json SOMNode::serialize() const
{
	json j;
	j["type"] = static_cast<int>(type);
	j["name"] = nodeName;
	j["id"] = uid;

	j["pose"] = {
		{"position", {{"x", localPose.position.x}, {"y", localPose.position.y}, {"z", localPose.position.z}}},
		{"orientation", {{"x", localPose.orientation.x}, {"y", localPose.orientation.y}, {"z", localPose.orientation.z}, {"w", localPose.orientation.w}}},
		{"scale", {{"x", localPose.scale.x}, {"y", localPose.scale.y}, {"z", localPose.scale.z}}}};

	j["physics"] = {
		{"mass", physics.mass},
		{"isStatic", physics.isStatic},
		{"useGravity", physics.useGravity},
		{"friction", physics.friction},
		{"restitution", physics.restitution},
		{"collisionShape", physics.collisionShape}};

	j["attributes"] = attributes;

	j["children"] = json::array();
	for (const auto &child : childNodes)
	{
		j["children"].push_back(child->serialize());
	}
	return j;
}

std::shared_ptr<SOMNode> SOMNode::deserialize(const json &j)
{
	auto node = std::make_shared<SOMNode>(
		static_cast<NodeType>(j["type"].get<int>()),
		j["name"].get<std::string>());

	node->uid = j["id"].get<uint64_t>();

	const auto &pose = j["pose"];
	node->localPose.position = Vector3(
		pose["position"]["x"].get<double>(),
		pose["position"]["y"].get<double>(),
		pose["position"]["z"].get<double>());

	node->localPose.orientation = Quaternion(
		pose["orientation"]["x"].get<double>(),
		pose["orientation"]["y"].get<double>(),
		pose["orientation"]["z"].get<double>(),
		pose["orientation"]["w"].get<double>());

	node->localPose.scale = Vector3(
		pose["scale"]["x"].get<double>(),
		pose["scale"]["y"].get<double>(),
		pose["scale"]["z"].get<double>());

	const auto &physics = j["physics"];
	node->physics.mass = physics["mass"].get<double>();
	node->physics.isStatic = physics["isStatic"].get<bool>();
	node->physics.useGravity = physics["useGravity"].get<bool>();
	node->physics.friction = physics["friction"].get<double>();
	node->physics.restitution = physics["restitution"].get<double>();
	node->physics.collisionShape = physics["collisionShape"].get<std::string>();

	node->attributes = j["attributes"].get<std::unordered_map<std::string, std::string>>();

	for (const auto &childJson : j["children"])
	{
		node->appendChild(deserialize(childJson));
	}

	return node;
}