#include "DefaultHTTPService.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/ServerSettings.h"

// Doesn't link properly when WebRTC is linked...
//#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

using namespace teleport;
using namespace server;

DefaultHTTPService::DefaultHTTPService()
	: mUsingSSL(false)
{
	TELEPORT_INFO("DefaultHTTPService constructor.");
}

DefaultHTTPService::~DefaultHTTPService()
{
	TELEPORT_INFO("DefaultHTTPService destructor.");
	shutdown();
}

void httpLog(const httplib::Request &req, const httplib::Response &res)
{
	TELEPORT_INFO("Request:  {0}",req.path);
	TELEPORT_INFO("Response: {0} {1}",res.body,res.status);
}

bool DefaultHTTPService::initialize(std::string mountDirectory, std::string certPath, std::string privateKeyPath, int32_t port)
{
	if (mountDirectory == "")
	{
		TELEPORT_CERR << "Path of directory to mount cannot be empty!" << "\n";
		return false;
	}

	if (port == 0)
	{
		TELEPORT_CERR << "HTTP port cannot be 0!" << "\n";
		return false;
	}

	if (certPath.size() > 0 && privateKeyPath.size() > 0)
	{
		TELEPORT_COUT << "SSL certificate and key provided. Usng HTTPS server." << "\n";
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
		mServer.reset(new httplib::SSLServer(certPath.c_str(), privateKeyPath.c_str()));
		mUsingSSL = true;
#endif
	}
	else
	{
		TELEPORT_COUT << "SSL certificate and key not provided. Usng HTTP server." << "\n";
		mServer.reset(new httplib::Server());
		mUsingSSL = false;
	}



	// Mount / to provided directory
	auto ret = mServer->set_mount_point("/", mountDirectory+"/");
	if (!ret)
	{
		TELEPORT_CERR << "Provided mount directory does not exist!" << "\n";
		return false;
	}
	mServer->set_error_handler([](const auto& req, auto& res) {
		TELEPORT_WARN("HTTP Error: {0}",req.path);
	});
	/*mServer->set_file_request_handler([](const httplib::Request &req, httplib::Response &res) {
		TELEPORT_WARN("File Request: {0}",req.path);
		});*/
	mServer->set_exception_handler([](const auto& req, auto& res, std::exception &e) {
		  auto fmt = "<h1>Error 500</h1><p>%s</p>";
		  char buf[BUFSIZ];
		  try {
			std::rethrow_exception(std::make_exception_ptr(e));
		  } catch (std::exception &e) {
			snprintf(buf, sizeof(buf), fmt, e.what());
		  } catch (...) { // See the following NOTE
			snprintf(buf, sizeof(buf), fmt, "Unknown Exception");
		  }
	  res.set_content(buf, "text/html");
	  res.status = 500;
	});
	// TODO: Pass these extra mappings in as parameters.
	mServer->set_file_extension_and_mimetype_mapping("texture", "image/texture");
	mServer->set_file_extension_and_mimetype_mapping("mesh", "image/mesh");
	mServer->set_file_extension_and_mimetype_mapping("material", "image/material");
	mServer->set_file_extension_and_mimetype_mapping("basis", "image/basis");

	mServer->bind_to_port("0.0.0.0", port);
	mServer->set_logger(httpLog);
	if (!mThread.joinable())
	{
		mThread = std::thread(&httplib::Server::listen_after_bind, mServer.get());
	}

	return true;
}

void DefaultHTTPService::shutdown()
{
	if (!mServer)
		return;

	mServer->stop();
	if (mThread.joinable())
	{
		mThread.join();
	}
	mServer.reset();

	mUsingSSL = false;
}

void DefaultHTTPService::tick()
{

}
