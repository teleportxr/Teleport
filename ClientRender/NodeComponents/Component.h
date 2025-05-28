#pragma once

#include <map>
#include <memory>
#include <vector>

namespace teleport
{
	namespace clientrender
	{
		class Node;
		class Component
		{
		protected:
			Node &owner;
		public:
			Component(Node &owning_node): owner(owning_node) {}
			virtual ~Component() {}
		};
	}
}