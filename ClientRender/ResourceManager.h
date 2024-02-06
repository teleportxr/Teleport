// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <string>			//std::string
#include <functional>		//std::function
#include <vector>			//std::vector
#include <memory>			//Smart pointers
#include <mutex>			//Thread safety.
#include <algorithm>		//std::remove
#include <parallel_hashmap/phmap.h>
#include "MemoryUtil.h"
#include "Resource.h"
#include "TeleportCore/ErrorHandling.h"

typedef unsigned long long uid; //Unique identifier for a resource.

//A class for managing resources that are destroyed after a set amount of time.
//Get resources by claiming them, and then unclaim them when you no longer are using them; i.e. when the object instant is destructed.
template<typename u,class T>
class ResourceManager
{
	avs::uid cache_uid=0;
public:
	//Struct to keep the resource and its metadata together.
	struct ResourceData
	{
		std::shared_ptr<T> resource;
		float postUseLifetime_s; // Seconds the resource should be kept alive after the last object has stopped using it.
		float timeSinceLastUse_s; // Seconds since the data was last used by the session.
	};

	//Create a resource manager with the class specific function to free it from memory before destroying the resource.
	ResourceManager(avs::uid cache_uid,std::function<void(T &)> freeResourceFunction = nullptr);
	~ResourceManager();

	//Add a resource to the resource manager.
	//	id : Unique identifier of the resource.
	//	newResource : The resource.
	//	postUseLifetime : Milliseconds the resource should be kept alive after the last object has stopped using it.
	void Add(u id, std::shared_ptr<T> & newItem, float postUseLifetime_s = 60.0f);

	//Returns whether the manager contains the resource.
	bool Has(u id) const;
	
	//Returns the internal cache.
	//	cacheLock : A lock which must live for the same duration of the map, or will break thread-safety. 
	phmap::flat_hash_map<u, ResourceData>& GetCache(std::unique_ptr<std::lock_guard<std::mutex>>& cacheLock);
	
	const phmap::flat_hash_map<u, ResourceData>& GetCache(std::unique_ptr<std::lock_guard<std::mutex>>& cacheLock) const;

	//! Returns a shared pointer to the resource; returns nullptr if the resource was not found.
	// !Resets time since last use of the resource.
	std::shared_ptr<T> Get(u id);

	//! Returns the first object found which has the name given.
	u GetUidByName(const char *) const;

	//Returns a shared pointer to the resource; returns nullptr if the resource was not found.
	//Resets time since last use of the resource.
	std::shared_ptr<const T> Get(u id) const;

	//Pushes the IDs of all of the resources stored in the resource manager into the passed vector.
	const std::vector<u>& GetAllIDs() const;

	//Clear, and free memory of, all resources.
	void Clear();
	//Clear, and free memory of, all resources; bar from resources on the list.
	//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
	void ClearAllButExcluded(std::vector<u>& excludeList);

	//Process the ResourceManager for this tick; allowing it to free any resources that have not been used for a while.
	//	deltaTimestamp : Milliseconds that have passed since the last update.
	void Update(float deltaTimestamp,float lifetimeFactor);
private:

	mutable std::vector<u> resourceIDs;
	mutable uint64_t cacheChecksum=0;
	mutable uint64_t idListChecksum=0;
	//Increases readability by obfuscating the full iterator definition.
	typedef typename phmap::flat_hash_map<u, ResourceManager<u,T>::ResourceData>::iterator mapIterator_t;

	std::function<void(T&)> freeResourceFunction; //A functional reference to the function that frees this resource.
	phmap::flat_hash_map<u, ResourceData> cachedItems = phmap::flat_hash_map<u, ResourceData>(); //Hashmap of the stored resources.

	mutable std::mutex mutex_cachedItems; //Mutex for thread-safety of cachedItems.

	//Frees the resource using the function that was passed to the resource manager on construction
	void FreeResource(T & resource);
	//Remove, and free the memory of, the item the iterator is pointing to.
	//	it : Iterator pointing to the item we want to delete.
	//Returns an iterator to the next item in the unordered map.
	mapIterator_t RemoveResource(mapIterator_t it);

	// Bytes
	static constexpr long MIN_REQUIRED_MEMORY = 1000000;
};

template<typename u,class T>
ResourceManager<u,T>::ResourceManager(avs::uid c_uid,std::function<void(T&)> freeResourceFunction)
	: cache_uid(c_uid), freeResourceFunction(freeResourceFunction)
{}

template<typename u,class T>
ResourceManager<u,T>::~ResourceManager()
{
	Clear();
}

template<typename u,class T>
void ResourceManager<u,T>::Add(u id, std::shared_ptr<T> & newItem, float postUseLifetime_s)
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);
	ResourceData resd = {newItem, postUseLifetime_s, 0};
	auto res= cachedItems.emplace(id, resd);
	if(res.second==false)
//	if(newkey==cachedItems.end())
	{
		cachedItems[id]=resd;
	}

	cacheChecksum++;
}

template<typename u,class T> bool ResourceManager<u,T>::Has(u id) const
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);
	return cachedItems.find(id) != cachedItems.end();
}

template<typename u,class T> inline phmap::flat_hash_map<u, typename ResourceManager<u,T>::ResourceData>& ResourceManager<u,T>::GetCache(std::unique_ptr<std::lock_guard<std::mutex>>& cacheLock)
{
	cacheLock = std::make_unique<std::lock_guard<std::mutex>>(mutex_cachedItems);
	return cachedItems;
}

template<typename u,class T> inline const phmap::flat_hash_map<u, typename ResourceManager<u,T>::ResourceData>& ResourceManager<u,T>::GetCache(std::unique_ptr<std::lock_guard<std::mutex>>& cacheLock) const
{
	cacheLock = std::make_unique<std::lock_guard<std::mutex>>(mutex_cachedItems);
	return cachedItems;
}

template<typename u,class T> u ResourceManager<u,T>::GetUidByName(const char *n) const
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);
	std::string name=n;
	for(const auto &i:cachedItems)
	{
		if(i.second.resource->name==name)
			return i.first;
	}
	return 0;
}
template<typename u,class T> std::shared_ptr<T> ResourceManager<u,T>::Get(u id)
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);

	auto it=cachedItems.find(id);
	if(it==cachedItems.end())
		return nullptr;

	ResourceData& data = it->second;

	data.timeSinceLastUse_s = 0;

	return data.resource;
}

template<typename u,class T> std::shared_ptr<const T> ResourceManager<u,T>::Get(u id) const
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);

	auto it = cachedItems.find(id);
	if (it == cachedItems.end())
		return nullptr;

	const ResourceData& data = it->second;

	return data.resource;
}

template<typename u,class T> const std::vector<u>&  ResourceManager<u,T>::GetAllIDs() const
{
	if(cacheChecksum!=idListChecksum)
	{
		resourceIDs.clear();
		std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);

		for(auto idDataPair : cachedItems)
		{
			resourceIDs.push_back(idDataPair.first);
		}
		idListChecksum=cacheChecksum;
	}
	return resourceIDs;
}

template<typename u,class T> void ResourceManager<u,T>::Clear()
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);
	for(auto &[id, data] : cachedItems)
	{
		FreeResource(*data.resource);
	}
	resourceIDs.clear();
	cachedItems.clear();
	cacheChecksum++;
	idListChecksum++;
}

template<typename u,class T> void ResourceManager<u,T>::ClearAllButExcluded(std::vector<u>& excludeList)
{
	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);
	for(auto it = cachedItems.begin(); it != cachedItems.end();)
	{
		bool isExcluded = false; //We don't remove the resource if it is excluded.
		unsigned int i = 0;
		while(i < excludeList.size() && !isExcluded)
		{
			//The resource is excluded if its id appears in the exclude list.
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
			excludeList.erase(std::remove(excludeList.begin(), excludeList.end(), excludeList[i - 1]), excludeList.end());
		}
		//Remove the resource if it is not.
		else
		{
			it = RemoveResource(it);
		}
	}
	cacheChecksum++;
}

template<typename u,class T>
void ResourceManager<u,T>::Update(float deltaTimestamp_s,float lifetimeFactor)
{
	if(!lifetimeFactor)
		return;
	const bool sufficientMemory = false;//clientrender::MemoryUtil::Get()->isSufficientMemory(MIN_REQUIRED_MEMORY);

	std::lock_guard<std::mutex> lock_cachedItems(mutex_cachedItems);
	//We will be deleting any resources that have lived without being used for more than their allowed lifetime.
	for(auto it = cachedItems.begin(); it != cachedItems.end();)
	{
		//Increment time spent unused, if the resource manager is the only object pointing to the resource.
		if(it->second.resource.use_count() == 1)
		{
			it->second.timeSinceLastUse_s += deltaTimestamp_s;

			//Delete the resource, if memory is low and it has been too long since the object was last used.
			if(!sufficientMemory && it->second.timeSinceLastUse_s >= it->second.postUseLifetime_s * lifetimeFactor)
			{
				TELEPORT_INTERNAL_CERR("Cache {0}, Timeout Freeing {1} resource {2} ({3})\n",cache_uid,T::getTypeName(),it->first,it->second.resource->getName());
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

template<typename u,class T>
void ResourceManager<u,T>::FreeResource(T & resource)
{
	if(freeResourceFunction)
	{
		freeResourceFunction(resource);
	}
}

template<typename u,class T>
typename ResourceManager<u,T>::mapIterator_t ResourceManager<u,T>::RemoveResource(typename ResourceManager<u,T>::mapIterator_t it)
{
	FreeResource(*it->second.resource);
	cacheChecksum++;
	return cachedItems.erase(it);
}
