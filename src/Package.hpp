#ifndef PACKAGE_H
#define PACKAGE_H
#include "Manifest.hpp"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include <string>
#if defined(SWITCH) || defined(WII)
#define ROOT_PATH "/"
#elif defined(__WIIU__)
#define ROOT_PATH "fs:/vol/external01/"
#elif defined(_3DS)
#define ROOT_PATH "sdmc:/"
#else
#define ROOT_PATH "sdroot/"
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#define APP_SHORTNAME "appstore"

class Repo;

/** 
 * A package is a single application that can be installed. It contains the URL to the zip file and any instructions to install it (like a GET manifest).
 * 
 * The download and install process is handled here, but they will use logic in the parentRepo's class to get the zip URL and installation logic.
*/
class Package
{
public:
	Package(int state);
	~Package();

	std::string toString();
	bool downloadZip(const char* tmp_path, float* progress = NULL);
	bool install(const char* pkg_path, const char* tmp_path);
	bool remove(const char* pkg_path);
	const char* statusString();
	void updateStatus(const char* pkg_path);

	std::string getIconUrl();
	std::string getBannerUrl();
	std::string getManifestUrl();
	std::string getScreenShotUrl(int count);

	int isPreviouslyInstalled();

	// Package attributes
	std::string pkg_name;
	std::string title;
	std::string author;
	std::string short_desc;
	std::string long_desc;
	std::string version;

	std::string license;
	std::string changelog;
	
	std::string url;
	std::string sourceUrl;
	std::string iconUrl;

	std::string updated;
	std::string binary;

	Manifest* manifest = NULL;
	int updated_timestamp = 0;

	int downloads = 0;
	int extracted_size = 0;
	int download_size = 0;
	int screens = 0;

	std::string category;

	// Sorting attributes
	Repo* parentRepo;
	std::string* repoUrl;

	int status; // local, update, installed, get

	// bitmask for permissions, from left to right:
	// unused, iosu, kernel, nand, usb, sd, wifi, sound
	char permissions;

	// the downloaded contents file, to keep memory around to cleanup later
	std::string* contents;
};

#endif
