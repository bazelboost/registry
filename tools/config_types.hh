#pragma once

#include "nlohmann/json.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace bazel_registry {
struct metadata_config {
	struct maintainer_config {
		std::string email;
		std::string github;
		std::string name;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(maintainer_config, email, github, name)
	};

	std::string                                  homepage;
	std::vector<maintainer_config>               maintainers;
	std::vector<std::string>                     repository;
	std::vector<std::string>                     versions;
	std::unordered_map<std::string, std::string> yanked_versions;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		metadata_config,
		homepage,
		maintainers,
		repository,
		versions,
		yanked_versions
	)
};

struct source_config {
	std::string                                  integrity;
	int                                          patch_strip;
	std::unordered_map<std::string, std::string> patches;
	std::string                                  url;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		source_config,
		integrity,
		patch_strip,
		patches,
		url
	)
};
} // namespace bazel_registry
