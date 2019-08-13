#pragma once

#include <string> //std::string
#include <unordered_map> //std::unordered_map
#include <functional> //std::function
#include <vector> //std::vector

typedef unsigned long long uid; //Unique identifier for a resource.

//A class for managing resources that are destroyed after a set amount of time.
//Get resources by claiming them, and then unclaim them when you no longer are using them; i.e. when the object instant is destructed.
template<class T>
class ResourceManager
{
public:
	//Create a resource manager with the class specific function to free it from memory before destroying the resource.
	ResourceManager(std::function<void(T&)> freeResourceFunction = nullptr);
	~ResourceManager();

	//Add a resource to the resource manager.
	//	id : Unique identifier of the resource.
	//	newResource : The resource.
	//	postUseLifetime : Milliseconds the resource should be kept alive after the last object has stopped using it.
	void Add(uid id, T&& newResource, uint32_t postUseLifetime = 30000);

	//Returns whether the manager contains the resource.
	bool Has(uid id) const;
	
	//Set the factor to adjust the lifetime of resources before freeing them; i.e. 0.5 would halve the lifetime of a resource in the manager.
	void SetLifetimeFactor(float lifetimeFactor);

	//Claim usage of a resource, while retrieving a pointer to the resource; returns nullptr if the resource was not found.
	//You must use Unclaim once you are finished with a resource, so that the resource manager may clean up the resource when it is no longer needed.
	const T* Claim(uid id);
	//Unclaim the resource, so it may be freed after a set amount of time.
	void Unclaim(uid id);

	//Clear, and free memory of, all resources.
	void Clear();
	//Clear, and free memory of, all resources; bar from resources on the list.
	//	excludeList : Elements to not clear from the manager.
	void ClearCareful(std::vector<uid> excludeList);

	//Process the ResourceManager for this tick; allowing it to free any resources that have not been used for a while.
	//	deltaTimestamp : Milliseconds that have passed since the last update.
	void Update(uint32_t deltaTimestamp);
private:
	//Struct to keep the resource and its metadata together.
	struct ResourceData
	{
		uint32_t postUseLifetime; //Milliseconds the resource should be kept alive after the last object has stopped using it.

		T resource;
		unsigned int claimCount; //Amount of objects claiming use of this resource.
		uint32_t timeSinceLastUse; //Milliseconds since the data was last used by the session.
	};

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
void ResourceManager<T>::Add(uid id, T && newItem, uint32_t postUseLifetime)
{
	cachedItems[id] = {postUseLifetime, std::move(newItem), 0, 0};
}

template<class T>
bool ResourceManager<T>::Has(uid id) const
{
	return cachedItems.find(id) != cachedItems.end();
}

template<class T>
void ResourceManager<T>::SetLifetimeFactor(float lifetimeFactor)
{
	this->lifetimeFactor = lifetimeFactor;
}

template<class T>
const T* ResourceManager<T>::Claim(uid id)
{
	try
	{
		//Will throw if no resource has the passed id.
		ResourceData &data = cachedItems.at(id);

		data.timeSinceLastUse = 0;
		++data.claimCount;

		return &data.resource;
	}
	//Return nullptr if the value doesn't exist.
	catch(std::out_of_range oor)
	{
		return nullptr;
	}
}

template<class T>
void ResourceManager<T>::Unclaim(uid id)
{
	--cachedItems[id].claimCount;
}

template<class T>
void ResourceManager<T>::Clear()
{
	for(auto &[id, data] : cachedItems)
	{
		FreeResource(data.resource);
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
		if(it->second.claimCount == 0)
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
	FreeResource(it->second.resource);
	return cachedItems.erase(it);
}
