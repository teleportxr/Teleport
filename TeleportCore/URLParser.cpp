#include "URLParser.h"
#include "ErrorHandling.h"
#include <regex>
#include <format>
using std::string;
using std::regex;

using namespace teleport;
using namespace core;

DomainPortPath teleport::core::GetDomainPortPath(const std::string &url)
{
	using std::string, std::regex;
	DomainPortPath domainPortPath;
	domainPortPath.protocol = "teleport";
	try
	{  
		string protocol_str = R"((?:([^:\/?#]+):\/\/))";
		string domain_str = R"(([-a-zA-Z0-9@%_\+~#=.]+))";
		string port_str = R"((?::([0-9]+)))";
		string path_str = R"((?:\/([-a-zA-Z0-9@:%_\+~#=]+(?:\/[-a-zA-Z0-9@:%_\+~#=]+)*)\/?))";
		string param_str = R"((?:\?([-a-zA-Z0-9@:%_\+~#=,\/.]+(?:&[-a-zA-Z0-9@:%_\+~#=,\/.]+)*)\/?))";
		string re_str = std::format(R"(^{0}?{1}{2}?{3}?{4}?)", protocol_str, domain_str, port_str, path_str, param_str);
		regex re(re_str, std::regex_constants::icase);
		std::smatch match;
		if (std::regex_search(url, match, re))
		{
			TELEPORT_INTERNAL_COUT(Default, "Domain regex:");
			for (size_t m=0;m<match.size();m++)
			{
				TELEPORT_INTERNAL_COUT(Default, "\t{}: {}", m, match[m].str());
			}
		}
		domainPortPath.domain = (match.size() > 2 && match[2].matched) ? match[2].str() : "";
		try
		{
			domainPortPath.port = std::stoi((match.size() > 3 && match[3].matched) ? match[3].str() : "0");
		}
		catch (...)
		{
			domainPortPath.port = 0;
		}
		if (match.size() > 1 && match[1].matched)
		{
			domainPortPath.protocol = match[1].str();
		}
		if (match.size() > 4 && match[4].matched)
		{
			domainPortPath.path = match[4].str();
		}
	}
	catch (std::regex_error err)
	{
		TELEPORT_INTERNAL_CERR("Regex error for url {}: {}", url, err.what());
	}
	catch (...)
	{
		TELEPORT_INTERNAL_CERR("Regex error for url {}", url);
	}
	return domainPortPath;
}