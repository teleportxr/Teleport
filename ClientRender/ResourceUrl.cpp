// (C) Copyright 2018-2026 Simul Software Ltd
#include "ResourceUrl.h"
#include <vector>

std::string teleport::clientrender::ResolveUrl(const std::string &base, const std::string &relative)
{
	if (relative.empty())
	{
		return relative;
	}
	if (relative.find("://") != std::string::npos)
	{
		return relative;
	}
	// A base with no scheme gives us nothing to resolve against; the caller's own url-root
	// handling is then the only thing that can complete it.
	size_t schemeEnd = base.find("://");
	if (schemeEnd == std::string::npos)
	{
		return relative;
	}
	// Query and fragment belong to the base document, not to the path we resolve against.
	std::string basePath	 = base.substr(0, base.find_first_of("?#"));
	size_t		authorityEnd = basePath.find('/', schemeEnd + 3);
	const std::string origin = (authorityEnd == std::string::npos) ? basePath : basePath.substr(0, authorityEnd);

	if (relative[0] == '/')
	{
		return origin + relative;
	}

	// Relative to the directory the base document sits in.
	std::string directory = (authorityEnd == std::string::npos) ? std::string() : basePath.substr(authorityEnd + 1);
	size_t		lastSlash = directory.rfind('/');
	directory			  = (lastSlash == std::string::npos) ? std::string() : directory.substr(0, lastSlash + 1);

	// Resolve "." and ".." against the segments we have, as RFC 3986 does.
	std::vector<std::string> segments;
	const std::string		 combined = directory + relative;
	size_t					 start	  = 0;
	while (start <= combined.size())
	{
		const size_t	  slash	  = combined.find('/', start);
		const bool		  last	  = (slash == std::string::npos);
		const std::string segment = combined.substr(start, last ? std::string::npos : slash - start);
		if (segment == "..")
		{
			if (!segments.empty())
			{
				segments.pop_back();
			}
		}
		else if (segment == ".")
		{
			// Current directory: contributes nothing.
		}
		else if (!segment.empty() || last)
		{
			// An empty final segment is a trailing slash, which is meaningful; an empty interior
			// one is a doubled slash, which is not.
			segments.push_back(segment);
		}
		if (last)
		{
			break;
		}
		start = slash + 1;
	}

	std::string path;
	for (const std::string &segment : segments)
	{
		path += "/";
		path += segment;
	}
	if (path.empty())
	{
		path = "/";
	}
	return origin + path;
}
