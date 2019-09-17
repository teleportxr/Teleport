// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <string> //std::string
#include <unordered_map> //std::unordered_map
#include <functional> //std::function
#include <vector> //std::vector
#include <memory> //Smart pointers

typedef unsigned long long uid; //Unique identifier for a resource.

//A class for managing resources that are destroyed after a set amount of time.
//Get resources by claiming them, and then unclaim them when you no longer are using them; i.e. when the object instant is destructed.
template<class T>
class ResourceManager
{
public:
	//Struct to keep the resource and its metadata together.
	struct ResourceData
	{
		std::shared_ptr<T> resource;
		uint32_t postUseLifetime; //Milliseconds the resource should be kept alive after the last object has stopped using it.
		uint32_t timeSinceLastUse; //Milliseconds since the data was last used by the session.
	};

	//Create a resource manager with the class specific function to free it from memory before destroying the resource.
	ResourceManager(std::function<void(T&)> freeResourceFunction = nullptr);
	~ResourceManager();

	//Add a resource to the resource manager.
	//	id : Unique identifier of the resource.
	//	newResource : The resource.
	//	postUseLifetime : Milliseconds the resource should be kept alive after the last object has stopped using it.
	void Add(uid id, std::shared_ptr<T> & newItem, uint32_t postUseLifetime = 1000);

	//Returns whether the manager contains the resource.
	bool Has(uid id) const;
	
	/// Returns the internal cache.
	std::unordered_map<uid, ResourceData>& GetCache();

	//Set the factor to adjust the lifetime of resources before freeing them; i.e. 0.5 would halve the lifetime of a resource in the manager.
	void SetLifetimeFactor(float lifetimeFactor);

	//Returns a shared pointer to the resource; returns nullptr if the resource was not found.
	//Resets time since last use of the resource.
	std::shared_ptr<T> Get(uid id);

	//Clear, and free memory of, all resources.
	void Clear();
	//Clear, and free memory of, all resources; bar from resources on the list.
	//	excludeList : Elements to not clear from the manager.
	void ClearCareful(std::vector<uid> excludeList);

	//Process the ResourceManager for this tick; allowing it to free any resources that have not been used for a while.
	//	deltaTimestamp : Milliseconds that have passed since the last update.
	void Update(uint32_t deltaTimestamp);
private:

	//Increases readability by obfuscating the full iterator definition.
	typedef typename std::unordered_map<uid, ResourceManager<T>::ResourceData>::iterator mapIterator_t;

	float lifetimeFactor = 1.0; //The factor lifetimes are adjusted to determine if a resource should be freed. 0.5 = Halve lifetime.
	std::function<void(T&)> freeResourceFunction; //A functional reference to the function that frees this resource.
	std::unordered_map<uid, ResourceData> cachedItems = std::unordered_map<uid, ResourceData>(); //Hashmap of the stored resources.

	//Frees the resource using the function that was passed to the resource manager on construction
	void FreeResource(T & resource);
	//Remove, and free the memory of, the item the iterator is pointing to.
	//	it : Iterator pointing to the item we want to delete.
	//Returns an iterator to the next item in the unordered map.
	mapIterator_t RemoveResource(mapIterator_t it);
};

template<class T>
ResourceManager<T>::ResourceManager(std::function<void(T&)> freeResourceFunction)
	:freeResourceFunction(freeResourceFunction)
{}

template<class T>
ResourceManager<T>::~ResourceManager()
{
	Clear();
}

template<class T>
void ResourceManager<T>::Add(uid id, std::shared_ptr<T> & newItem, uint32_t postUseLifetime)
{
	cachedItems.emplace(id, ResourceData{newItem, postUseLifetime, 0});
}

template<class T>
bool ResourceManager<T>::Has(uid id) const
{
	return cachedItems.find(id) != cachedItems.end();
}

template<class T>
inline std::unordered_map<uid, typename ResourceManager<T>::ResourceData>& ResourceManager<T>::GetCache()
{
	return cachedItems;
}

template<class T>
void ResourceManager<T>::SetLifetimeFactor(float lifetimeFactor)
{
	this->lifetimeFactor = lifetimeFactor;
}

template<class T>
std::shared_ptr<T> ResourceManager<T>::Get(uid id)
{
	try
	{
		//Will throw if no resource has the passed id.
		ResourceData& data = cachedItems.at(id);

		data.timeSinceLastUse = 0;

		return data.resource;
	}
	//Return nullptr if the value doesn't exist.
	catch(std::out_of_range oor)
	{
		return nullptr;
	}
}

template<class T>
void ResourceManager<T>::Clear()
{
	for(auto &[id, data] : cachedItems)
	{
		FreeResource(*data.resource);
	}

	cachedItems.clear();
}

template<class T>
void ResourceManager<T>::ClearCareful(std::vector<uid> excludeList)
{
	for(auto it = cachedItems.begin(); it != cachedItems.end();)
	{
		bool isExcluded = false; //We don't remove the resource if it is excluded.
		unsigned int i = 0;
		while(i < excludeList.size() && !isExcluded)
		{
			//The resource is excluded if its uid appears in the exclude list.
			if(excludeList[i] == it->first)
			{
				isExcluded = true;
			}

			++i;
		}

		//Increment the iterator if it is excluded.
		if(isExcluded)
		{
			++it;
		}
		//Remove the resource if it is not.
		else
		{
			it = RemoveResource(it);
		}
	}
}

template<class T>
void ResourceManager<T>::Update(uint32_t deltaTimestamp)
{
	//We will be deleting any resources that have lived without being used for more than their allowed lifetime.
	for(auto it = cachedItems.begin(); it != cachedItems.end();)
	{
		//Increment time spent unused, if the resource manager is the only object pointing to the resource.
		if(it->second.resource.use_count() == 1)
		{
			it->second.timeSinceLastUse += deltaTimestamp;

			//Delete the resource, if it has been too long since the object was last used.
			if(it->second.timeSinceLastUse >= it->second.postUseLifetime * lifetimeFactor)
			{
				it = RemoveResource(it);
			}
			else
			{
				++it;
			}
		}
		else
		{
			++it;
		}
	}
}

template<class T>
void ResourceManager<T>::FreeResource(T & resource)
{
	if(freeResourceFunction)
	{
		freeResourceFunction(resource);
	}
}

template<class T>
typename ResourceManager<T>::mapIterator_t ResourceManager<T>::RemoveResource(typename ResourceManager<T>::mapIterator_t it)
{
	FreeResource(*it->second.resource);
	return cachedItems.erase(it);
}
