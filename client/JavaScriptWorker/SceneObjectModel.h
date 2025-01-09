#include "json.hpp"
#include <Windows.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
using json = nlohmann::json;
#include "libplatform/libplatform.h"
#include "v8.h"

struct Vector3
{
	double x, y, z;

	Vector3() : x(0), y(0), z(0) {}
	Vector3(double x, double y, double z) : x(x), y(y), z(z) {}

	double length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}

	Vector3 normalize() const
	{
		double len = length();
		return Vector3(x / len, y / len, z / len);
	}
};

struct Quaternion
{
	double x, y, z, w;

	Quaternion() : x(0), y(0), z(0), w(1) {}
	Quaternion(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}

	Quaternion inverse() const
	{
		return Quaternion(-x, -y, -z, w);
	}
};

struct Pose
{
	Vector3 position;
	Quaternion orientation;
	Vector3 scale;

	Pose() : scale(1, 1, 1) {}
};

struct PhysicsProperties
{
	double mass{1.0};
	bool isStatic{false};
	bool useGravity{true};
	double friction{0.5};
	double restitution{0.5};
	std::string collisionShape{"box"};
};

class Ray
{
public:
	Vector3 origin;
	Vector3 direction;

	Ray(const Vector3 &o, const Vector3 &d) : origin(o), direction(d.normalize()) {}
};

enum class NodeType
{
	NODE = 1,
	SCENE = 9
};

class SOMObserver
{
public:
	virtual void onNodeAdded(const std::shared_ptr<class SOMNode> &node) = 0;
	virtual void onNodeRemoved(const std::shared_ptr<class SOMNode> &node) = 0;
	virtual void onNodeMoved(const std::shared_ptr<class SOMNode> &node) = 0;
	virtual void onAttributeChanged(const std::shared_ptr<class SOMNode> &node, const std::string &name) = 0;
	virtual void onPhysicsUpdated(const std::shared_ptr<class SOMNode> &node) = 0;
	virtual ~SOMObserver() = default;
};
// Base SOM Object class

class SOMNode :  public std::enable_shared_from_this<SOMNode>
{
private:
	NodeType type;
	std::string nodeName;
	std::unordered_map<std::string, std::string> attributes;
	Pose localPose;
	Pose worldPose;
	std::weak_ptr<SOMNode> parentNode;
	std::vector<std::shared_ptr<SOMNode>> childNodes;
	PhysicsProperties physics;
	std::vector<std::shared_ptr<SOMObserver>> observers;
	uint64_t uid=0;
	bool transformDirty{true};

public:
	SOMNode(NodeType t, const std::string &name) : type(t), nodeName(name) {}
	virtual ~SOMNode() = default;

	NodeType getType() const { return type; }
    uint64_t getUid() const 
    { 
        return uid; 
    }
	const std::string &getNodeName() const { return nodeName; }

	std::shared_ptr<SOMNode> appendChild(std::shared_ptr<SOMNode> child)
	{
		child->parentNode = shared_from_this();
		childNodes.push_back(child);
		return child;
	}

	const std::vector<std::shared_ptr<SOMNode>> &getChildNodes() const
	{
		return childNodes;
	}

	void setPose(const Pose &newPose)
	{
		localPose = newPose;
		markTransformDirty();
		notifyObservers([this](auto &obs)
						{ obs->onNodeMoved(shared_from_this()); });
	}
	const Pose &getPose() const
	{
		return localPose;
	}

	const Pose &getLocalPose() const
	{
		return localPose;
	}

	Pose getWorldPose()
	{
		if (transformDirty)
		{
			updateWorldTransform();
		}
		return worldPose;
	}

	void setPhysicsProperties(const PhysicsProperties &props)
	{
		physics = props;
		notifyObservers([this](auto &obs)
						{ obs->onPhysicsUpdated(shared_from_this()); });
	}

	const PhysicsProperties &getPhysicsProperties() const
	{
		return physics;
	}


	bool intersectRay(const Ray &ray, double &distance) const
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

	json serialize() const
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

	static std::shared_ptr<SOMNode> deserialize(const json &j)
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

	std::string toJsonString() const
	{
		return serialize().dump(2); // 2 spaces for indentation
	}

	static std::shared_ptr<SOMNode> fromJsonString(const std::string &jsonStr)
	{
		try
		{
			json j = json::parse(jsonStr);
			return deserialize(j);
		}
		catch (const json::exception &e)
		{
			throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
		}
	}

private:
	void markTransformDirty()
	{
		transformDirty = true;
		for (auto &child : childNodes)
		{
			child->markTransformDirty();
		}
	}

	void updateWorldTransform()
	{
		if (auto parent = parentNode.lock())
		{
			// Combine parent's world transform with local transform
			Pose parentWorld = parent->getWorldPose();
			// Implement proper transform combination here
			worldPose = combineTransforms(parentWorld, localPose);
		}
		else
		{
			worldPose = localPose;
		}
		transformDirty = false;
	}

	Pose combineTransforms(const Pose &parent, const Pose &local)
	{
		Pose result;
		// Implement proper transform combination
		// This is a simplified version
		result.position.x = parent.position.x + local.position.x;
		result.position.y = parent.position.y + local.position.y;
		result.position.z = parent.position.z + local.position.z;
		// Proper quaternion multiplication needed here
		result.orientation = local.orientation; // Simplified
		result.scale.x = parent.scale.x * local.scale.x;
		result.scale.y = parent.scale.y * local.scale.y;
		result.scale.z = parent.scale.z * local.scale.z;
		return result;
	}

	template <typename F>
	void notifyObservers(F notification)
	{
		for (auto &observer : observers)
		{
			notification(observer);
		}
	}
};

//! A special SOM Node representing the scene. A Scene must have precisely *one* child, which is the root node.
//! This is the node that the script is attached to.
class Scene : public SOMNode
{
private:
	std::unordered_map<uint64_t, std::weak_ptr<SOMNode>> idMap;
	std::vector<std::shared_ptr<SOMNode>> spatialQueryResults;

public:
	Scene() : SOMNode(NodeType::SCENE, "#scene")
	{
	}

	std::shared_ptr<SOMNode> createNode(std::shared_ptr<SOMNode> parent,const std::string &name)
	{
		auto node = std::make_shared<SOMNode>(NodeType::NODE, name);
		parent->appendChild(node);
		return node;
	}
	std::shared_ptr<SOMNode> getRootNode()
	{
		if(getChildNodes().empty())
			return nullptr;
		return getChildNodes()[0];
	}

	std::shared_ptr<SOMNode> getElementById(const uint64_t &id)
	{
		auto it = idMap.find(id);
		if (it != idMap.end())
		{
			return it->second.lock();
		}
		return nullptr;
	}

	std::vector<std::shared_ptr<SOMNode>> querySelectorAll(const std::string &selector)
	{
		spatialQueryResults.clear();
		// Basic selector implementation - supports only tag names and IDs
		if (selector[0] == '#')
		{
			auto nodes = getNodesByName(selector.substr(1));
			for(auto n:nodes)
				if (n) spatialQueryResults.push_back(n);
		}
		else
		{
			queryByTagName(shared_from_this(), selector);
		}
		return spatialQueryResults;
	}

	std::shared_ptr<SOMNode> querySelector(const std::string &selector)
	{
		auto results = querySelectorAll(selector);
		return results.empty() ? nullptr : results[0];
	}

	std::vector<std::shared_ptr<SOMNode>> findNodesInRadius(const Vector3 &center, double radius)
	{
		spatialQueryResults.clear();
		findNodesInRadiusRecursive(shared_from_this(), center, radius);
		return spatialQueryResults;
	}

	std::vector<std::shared_ptr<SOMNode>> raycast(const Ray &ray)
	{
		spatialQueryResults.clear();
		raycastRecursive(shared_from_this(), ray);
		std::sort(spatialQueryResults.begin(), spatialQueryResults.end(),
				  [&ray](const auto &a, const auto &b)
				  {
					  double distA = 0.0;
					  double distB = 0.0;
					  a->intersectRay(ray, distA);
					  b->intersectRay(ray, distB);
					  return distA < distB;
				  });
		return spatialQueryResults;
	}
	
    std::vector<std::shared_ptr<SOMNode>> getNodesByName(const std::string& name)
    {
        std::vector<std::shared_ptr<SOMNode>> results;
        findNodesByNameRecursive(shared_from_this(), name, results);
        return results;
    }

private:
    static void findNodesByNameRecursive(const std::shared_ptr<SOMNode>& node, 
                                       const std::string& name,
                                       std::vector<std::shared_ptr<SOMNode>>& results)
    {
        if (node->getNodeName() == name)
        {
            results.push_back(node);
        }

        for (const auto& child : node->getChildNodes())
        {
            findNodesByNameRecursive(child, name, results);
        }
    }
	void queryByTagName(std::shared_ptr<SOMNode> rootObject, const std::string &tagName)
	{
		SOMNode *rootNode=static_cast<SOMNode*>(rootObject.get());
		for (auto &child : rootNode->getChildNodes())
		{
			if (child->getNodeName() == tagName)
			{
				spatialQueryResults.push_back(child);
			}
			queryByTagName(child, tagName);
		}
	}
	void findNodesInRadiusRecursive(const std::shared_ptr<SOMNode> &nodeObject,
									const Vector3 &center, double radius)
	{
		SOMNode *node=static_cast<SOMNode*>(nodeObject.get());
		Pose worldPose = node->getWorldPose();
		Vector3 toNode(
			worldPose.position.x - center.x,
			worldPose.position.y - center.y,
			worldPose.position.z - center.z);

		if (toNode.length() <= radius)
		{
			spatialQueryResults.push_back(nodeObject);
		}

		for (const auto &child : node->getChildNodes())
		{
			findNodesInRadiusRecursive(child, center, radius);
		}
	}

	void raycastRecursive(const std::shared_ptr<SOMNode> &nodeObject, const Ray &ray)
	{
		double distance;
		SOMNode *node=static_cast<SOMNode*>(nodeObject.get());
		if (node->intersectRay(ray, distance))
		{
			spatialQueryResults.push_back(nodeObject);
		}

		for (const auto &child : node->getChildNodes())
		{
			raycastRecursive(child, ray);
		}
	}
};
