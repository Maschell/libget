#include "LocalRepo.hpp"
#include "Get.hpp"
#include "Package.hpp"
#include "Utils.hpp"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include <cstring>
#include <dirent.h>

std::vector<std::unique_ptr<Package>> LocalRepo::loadPackages()
{
	std::vector<std::unique_ptr<Package>> result;
	// go through each folder in the .get/packages directory, and load their info
	DIR* dir;
	struct dirent* ent;
	std::string jsonPathInternal = "info.json";

	if ((dir = opendir(mPkg_path.c_str())) != nullptr)
	{
		while ((ent = readdir(dir)) != nullptr)
		{
			// skip . and ..
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			{
				continue;
			}
			if (is_dir(mPkg_path, ent))
			{
				auto pkg_dir = std::string(mPkg_path);
				pkg_dir.append(ent->d_name);
				pkg_dir.append("/");

				auto pkg = std::make_unique<Package>(LOCAL);
				pkg->pkg_name = ent->d_name;

				std::string jsonPath = mPkg_path + pkg->pkg_name + "/" + jsonPathInternal;

				// grab the info json
				struct stat sbuff
				{
				};
				if (stat(jsonPath.c_str(), &sbuff) == 0)
				{
					// pull out the version number and check if it's
					// different than the one on the repo
					std::ifstream ifs(jsonPath);
					rapidjson::IStreamWrapper isw(ifs);

					if (!ifs.good())
					{
						printf("--> Could not locate %s for local package folder", jsonPath.c_str());
						// so, we have a folder for this file, but no info.json
						// we can't in good faith do anything with this!
						continue;
					}

					rapidjson::Document doc;
					rapidjson::ParseResult ok = doc.ParseStream(isw);
					if (!ok)
					{
						printf("--> Could not parse %s for local package folder", jsonPath.c_str());
						continue;
					}

					// try to load as much info as possible from the info json
					// (normally, a get repo's package only uses the verison)

					if (doc.HasMember("title"))
					{
						pkg->title = doc["title"].GetString();
					}
					if (doc.HasMember("version"))
					{
						pkg->version = doc["version"].GetString();
					}
					if (doc.HasMember("author"))
					{
						pkg->author = doc["author"].GetString();
					}
					if (doc.HasMember("license"))
					{
						pkg->license = doc["license"].GetString();
					}
					if (doc.HasMember("details"))
					{
						pkg->short_desc = doc["description"].GetString();
					}
					if (doc.HasMember("description"))
					{
						pkg->long_desc = doc["details"].GetString();
					}
					if (doc.HasMember("changelog"))
					{
						pkg->changelog = doc["changelog"].GetString();
					}
				}

				pkg->status = LOCAL;

				// // TODO: load some other info from the json
				result.push_back(std::move(pkg));
			}
		}
		closedir(dir);
	}
	else
	{
		printf("--> Could not open packages directory\n");
	}
	return result;
}

std::string LocalRepo::getType() const
{
	return "local";
}

std::string LocalRepo::getZipUrl(const Package& package) const
{
	// Local packages don't have a zip url
	return "";
}

std::string LocalRepo::getIconUrl(const Package& package) const
{
	// ditto and/or ibid
	return "";
}