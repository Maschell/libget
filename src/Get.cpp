#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdarg>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "Get.hpp"
#include "Utils.hpp"
#include "GetRepo.hpp"
#include "LocalRepo.hpp"

using namespace std;
using namespace rapidjson;

bool debug = false;

Get::Get(const char* config_dir, const char* defaultRepo)
{
	this->defaultRepo = defaultRepo;

	// the path for the get metadata folder
	string config_path = config_dir;

	string* repo_file = new string(config_path + "repos.json");
	this->repos_path = repo_file->c_str();
	string* package_dir = new string(config_path + "packages/");
	this->pkg_path = package_dir->c_str();
	string* tmp_dir = new string(config_path + "tmp/");
	this->tmp_path = tmp_dir->c_str();

	//	  printf("--> Using \"./sdroot\" as local download root directory\n");
	//	  my_mkdir("./sdroot");

	my_mkdir(config_path.c_str());
	my_mkdir(package_dir->c_str());
	my_mkdir(tmp_dir->c_str());

	printf("--> Using \"%s\" as repo list\n", repo_file->c_str());

	// load repo info
	this->loadRepos();
	this->update();
}

int Get::install(Package* package)
{
	// found package in a remote server, fetch it
	bool located = package->downloadZip(this->tmp_path);

	if (!located)
	{
		// according to the repo list, the package zip file should've been here
		// but we got a 404 and couldn't find it
		printf("--> Error retrieving remote file for [%s] (check network or 404 error?)\n", package->pkg_name.c_str());
		return false;
	}

	// install the package, (extracts manifest, etc)
	package->install(this->pkg_path, this->tmp_path);

	printf("--> Downloaded [%s] to sdroot/\n", package->pkg_name.c_str());

	// update again post-install
	update();
	return true;
}

int Get::remove(Package* package)
{
	package->remove(this->pkg_path);
	printf("--> Uninstalled [%s] package\n", package->pkg_name.c_str());
	update();

	return true;
}

int Get::toggleRepo(Repo* repo)
{
	repo->setEnabled(!repo->isEnabled());
	update();
	return true;
}

void Get::addLocalRepo() {
	Repo* localRepo = new LocalRepo();
	repos.push_back(localRepo);
	update();
}

/**
Load any repos from a config file into the repos vector.
**/
void Get::loadRepos()
{
	repos.clear();
	const char* config_path = repos_path;

	ifstream* ifs = new ifstream(config_path);

	if (!ifs->good() || ifs->peek() == std::ifstream::traits_type::eof())
	{
		printf("--> Could not load repos from %s, generating default GET repos.json\n", config_path);

		Repo* defaultRepo = new GetRepo("Default Repo", this->defaultRepo, true);

		Document d;
		d.Parse(generateRepoJson(1, defaultRepo).c_str());

		std::ofstream file(config_path);
		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		d.Accept(writer);
		file << buffer.GetString();

		ifs = new ifstream(config_path);

		if (!ifs->good())
		{
			printf("--> Could not generate a new repos.json\n");

			// manually create a repo, no file access (so we append now, since we won't be able to load later)
			repos.push_back(defaultRepo);
			return;
		}
	}

	IStreamWrapper isw(*ifs);

	Document doc;
	ParseResult ok = doc.ParseStream(isw);

	if (!ok || !doc.HasMember("repos"))
	{
		printf("--> Invalid format in %s", config_path);
		return;
	}

	const Value& repos_doc = doc["repos"];

	// for every repo
	for (Value::ConstValueIterator it = repos_doc.Begin(); it != repos_doc.End(); it++)
	{

		auto repoName = "Default Repo";
		auto repoUrl = "";
		auto repoEnabled = false;
		auto repoType = "get";			// carryover from before this was defined

		if ((*it).HasMember("name"))
			repoName = (*it)["name"].GetString();
		if ((*it).HasMember("url"))
			repoUrl = (*it)["url"].GetString();
		if ((*it).HasMember("enabled"))
			repoEnabled = (*it)["enabled"].GetBool();
		if ((*it).HasMember("type"))
			repoType = (*it)["type"].GetString();

		printf("--> Found repo: %s, %s\n", repoName, repoType);

		Repo* repo = createRepo(repoName, repoUrl, repoEnabled, repoType);
		if (repo != NULL)
			repos.push_back(repo);
	}

	return;
}

void Get::update()
{
	printf("--> Updating package list\n");
	// clear current packages
	packages.clear();

	// fetch recent package list from enabled repos
	for (size_t x = 0; x < repos.size(); x++)
	{
		printf("--> Checking repo %s\n", repos[x]->getName().c_str());
		if (repos[x]->isLoaded() && repos[x]->isEnabled())
		{
			printf("--> Repo %s is loaded and enabled\n", repos[x]->getName().c_str());
			if (libget_status_callback != NULL)
				libget_status_callback(STATUS_RELOADING, x+1, repos.size());

			repos[x]->loadPackages(this, &packages);
		}
	}

	if (libget_status_callback != NULL)
		libget_status_callback(STATUS_UPDATING_STATUS, 1, 1);

  // remove duplicates, prioritizing later packages over earlier ones
  this->removeDuplicates();

	// check for any installed packages to update their status
	for (size_t x = 0; x < packages.size(); x++)
	{
		packages[x]->updateStatus(this->pkg_path);
	}
}

int Get::validateRepos()
{
	if (repos.size() == 0)
	{
		printf("--> There are no repos configured!\n");
		return ERR_NO_REPOS;
	}

	return 0;
}

std::vector<Package*> Get::search(std::string query)
{
	// TODO: replace with inverted index for speed
	// https://vgmoose.com/blog/implementing-a-static-blog-search-clientside-in-js-6629164446/

	std::vector<Package*> results = std::vector<Package*>();
	std::string lower_query = toLower(query);

	for (size_t x = 0; x < packages.size(); x++)
	{
		Package* cur = packages[x];
		if (cur != NULL && (toLower(cur->title).find(lower_query) != std::string::npos || toLower(cur->author).find(lower_query) != std::string::npos || toLower(cur->short_desc).find(lower_query) != std::string::npos || toLower(cur->long_desc).find(lower_query) != std::string::npos))
		{
			// matches, add to return vector, and continue
			results.push_back(cur);
			continue;
		}
	}

	return results;
}

Package* Get::lookup(std::string pkg_name)
{
	for (size_t x = 0; x < packages.size(); x++)
	{
		Package* cur = packages[x];
		if (cur != NULL && cur->pkg_name == pkg_name)
		{
			return cur;
		}
	}
	return NULL;
}

void Get::removeDuplicates()
{
  std::unordered_set<std::string> packageSet;
  std::unordered_set<Package*> removalSet;

  // going backards, fill out our sets
  // (prioritizes later repo packages over earlier ones, regardless of versioning)
  // TODO: semantic versioning or have a versionCode int that increments every update
  for (int x = packages.size()-1; x >= 0; x--)
  {
    std::string name = packages[x]->pkg_name;
    if (packageSet.find(name) == packageSet.end())
      packageSet.insert(name);
    else
      removalSet.insert(packages[x]);
  }

  // remove them, if they are in the removal set
  packages.erase(std::remove_if(packages.begin(), packages.end(), [removalSet](Package* p) {
    return removalSet.find(p) != removalSet.end();
  }), packages.end());
}

void info(const char* format, ...)
{
	if (!debug) return;
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}