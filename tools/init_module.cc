#include <filesystem>
#include <iostream>
#include <string>
#include <format>
#include <fstream>
#include <gflags/gflags.h>
#include "nlohmann/json.hpp"

#include "tools/config_types.hh"

namespace fs = std::filesystem;
using nlohmann::json;

DEFINE_string(module_name, "", "Boost module name");
DEFINE_validator(module_name, [](auto, auto module_name) -> bool {
	return !module_name.empty();
});

auto check_error(std::error_code ec) -> void {
	if(ec) {
		std::cerr << "ERROR: " << ec.message() << "\n";
		std::exit(1);
	}
}

auto main(int argc, char* argv[]) -> int {
	gflags::SetUsageMessage("Initialize a bazel boost module in the registry");
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	fs::current_path(std::getenv("BUILD_WORKSPACE_DIRECTORY"));

	auto ec = std::error_code{};
	auto metadata = bazel_registry::metadata_config{};
	auto metadata_config_path = fs::path{
		std::format("modules/boost.{}/metadata.json", FLAGS_module_name),
	};

	if(fs::exists(metadata_config_path)) {
		try {
			metadata = json::parse(std::ifstream{metadata_config_path});
		} catch(const json::parse_error& err) {
			std::cerr << err.what() << "\n";
		}
	} else {
		std::cout << std::format( //
			"Creating directory {}\n",
			metadata_config_path.parent_path().generic_string()
		);
		fs::create_directories(metadata_config_path.parent_path(), ec);
		check_error(ec);
	}

	metadata.homepage = std::format(
		"https://www.boost.org/doc/libs/release/libs/{}/doc/html/index.html",
		FLAGS_module_name
	);

	if(metadata.maintainers.empty()) {
		metadata.maintainers.push_back({
			.email = "ezekiel@seaube.com",
			.github = "zaucy",
			.name = "Ezekiel Warren",
		});
	}

	if(metadata.repository.empty()) {
		metadata.repository.push_back(
			std::format("github:bazelboost/{}", FLAGS_module_name)
		);
	}

	std::cout << std::format( //
		"Writing to {}\n",
		metadata_config_path.generic_string()
	);

	auto metadata_json = json{};
	to_json(metadata_json, metadata);

	auto metadata_config_file = std::ofstream{
		metadata_config_path,
		std::ios::trunc | std::ios::in | std::ios::out,
	};

	metadata_config_file << metadata_json.dump(2);
	metadata_config_file.flush();

	return 0;
}
