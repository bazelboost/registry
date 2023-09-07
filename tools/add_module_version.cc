#include <filesystem>
#include <iostream>
#include <array>
#include <string>
#include <format>
#include <fstream>
#include <string_view>
#include <ranges>
#include <algorithm>
#include <expected>
#include <sstream>
#include <gflags/gflags.h>
#include <openssl/evp.h>
#include "nlohmann/json.hpp"

#include "tools/config_types.hh"

namespace fs = std::filesystem;
using namespace std::string_view_literals;
using nlohmann::json;

DEFINE_string(archive, "", "archive url (or path for testing)");

auto check_error(std::error_code ec) -> void {
	if(ec) {
		std::cerr << "ERROR: " << ec.message() << "\n";
		std::exit(1);
	}
}

auto extract_archive(fs::path archive_path, fs::path out_dir) -> fs::path {
	auto ec = std::error_code{};
	auto archive_out_dir = out_dir /
		archive_path.filename().replace_extension("").replace_extension("");
	fs::remove_all(archive_out_dir, ec);
	fs::create_directories(archive_out_dir, ec);

	// TODO(zaucy): replace with libarchive or something else
	auto cmd = std::format(
		"tar -xf {} -C {}",
		archive_path.generic_string(),
		archive_out_dir.generic_string()
	);
	if(std::system(cmd.c_str()) != 0) {
		std::cerr << std::format(
			"Failed to extract archive {}",
			archive_path.generic_string()
		);
		std::exit(1);
	}

	return archive_out_dir;
}

auto download_archive(std::string archive_url) -> fs::path {
	auto archive = archive_url.substr(archive_url.find_last_of("/") + 1);
	// TODO(zaucy): replace with libcurl or something
	auto cmd = std::format("curl -L {} -o {}", archive_url, archive);

	if(std::system(cmd.c_str()) != 0) {
		std::cerr << std::format("Failed to download archive with\n\n\t{}\n", cmd);
		std::exit(1);
	}

	return fs::path{archive};
}

struct module_info {
	std::string name;
	std::string version;
	int         compatibility_level;
};

auto get_module_info(fs::path bzlmod_path) -> module_info {
	auto ec = std::error_code{};
	auto tmp_module_info_txt_path =
		fs::current_path() / ".cache" / "_tmp_module_info.txt";
	fs::create_directories(tmp_module_info_txt_path.parent_path(), ec);

	auto module_path = bzlmod_path / "MODULE.bazel";
	if(!fs::exists(module_path)) {
		std::cerr << std::format("{} does not exist\n", module_path.generic_string());
		std::exit(1);
	}

	auto cmd = std::format(
		"buildozer -root_dir=\"{}\" \"print name version compatibility_level\" "
		"//MODULE.bazel:all > \"{}\"",
		bzlmod_path.generic_string(),
		tmp_module_info_txt_path.string()
	);

	if(std::system(cmd.c_str()) != 0) {
		std::cerr << std::format(
			"Failed to get module info at {} with:\n\n\t{}\n",
			bzlmod_path.generic_string(),
			cmd
		);
		std::exit(1);
	}

	auto info = module_info{};

	auto tmp_module_info_txt_file = std::ifstream{tmp_module_info_txt_path};
	tmp_module_info_txt_file >> info.name >> info.version >>
		info.compatibility_level;
	tmp_module_info_txt_file.close();
	fs::remove(tmp_module_info_txt_path, ec);

	return info;
}

template<class CharContainer>
auto file_get_contents(const char* filename, CharContainer* v) -> size_t {
	::FILE* fp = ::fopen(filename, "rb");
	::fseek(fp, 0, SEEK_END);
	long sz = ::ftell(fp);
	v->resize(static_cast<typename CharContainer::size_type>(sz));
	if(sz) {
		::rewind(fp);
		::fread(&(*v)[0], 1, v->size(), fp);
	}
	::fclose(fp);
	return v->size();
}

template<class CharContainer>
auto file_get_contents(const char* filename) -> CharContainer {
	CharContainer cc;
	file_get_contents(filename, &cc);
	return cc;
}

auto calc_sha256_integrity(fs::path p)
	-> std::expected<std::string, std::string> {
	auto data = file_get_contents<std::vector<std::byte>>(p.string().c_str());
	auto ctx = EVP_MD_CTX_new();

	if(!ctx) {
		return std::unexpected("Failed to init evp ctx");
	}

	// TODO(zaucy): add some defer util library to bcr
	struct ctx_cleanup {
		EVP_MD_CTX* ctx;

		~ctx_cleanup() {
			EVP_MD_CTX_cleanup(ctx);
		}
	} _ctx_cleanup{ctx};

	if(!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
		return std::unexpected("Digest init failed");
	}

	if(!EVP_DigestUpdate(ctx, data.data(), data.size())) {
		return std::unexpected("Digest update failed");
	}

	uint8_t      hash[EVP_MAX_MD_SIZE];
	unsigned int hash_length = 0;

	if(!EVP_DigestFinal_ex(ctx, hash, &hash_length)) {
		return std::unexpected("Digest final failed");
	}

	auto b64_str = std::string{};
	b64_str.resize(hash_length * 4);

	auto b64_encode_size = EVP_EncodeBlock(
		reinterpret_cast<uint8_t*>(b64_str.data()),
		hash,
		hash_length
	);
	b64_str.resize(b64_encode_size);

	return "sha256-" + b64_str;
}

auto range_contains(auto&& range, auto&& value) -> bool {
	for(auto&& el : range) {
		if(el == value) {
			return true;
		}
	}

	return false;
}

auto main(int argc, char* argv[]) -> int {
	fs::current_path(std::getenv("BUILD_WORKSPACE_DIRECTORY"));
	gflags::SetUsageMessage("Add new version to module");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	if(FLAGS_archive.empty()) {
		std::cerr << "--archive is required\n";
		return 1;
	}

	auto ec = std::error_code{};
	auto archive = FLAGS_archive.starts_with("https://") //
		? download_archive(FLAGS_archive)
		: fs::path{FLAGS_archive};

	if(!fs::exists(archive)) {
		std::cerr << std::format( //
			"Archive file {} does not exist\n",
			archive.string()
		);
		return 1;
	}

	auto extract_dir =
		extract_archive(archive, fs::path{".cache"} / "module_archive");
	auto info = get_module_info(extract_dir);
	auto module_dir = fs::path{"modules"} / info.name;
	auto metadata_path = module_dir / "metadata.json";

	if(!info.name.starts_with("boost.")) {
		std::cerr << std::format(
			"Only boost modules may use this script. Instead got {}\n",
			info.name
		);
		return 1;
	}

	auto boost_module = info.name.substr("boost."sv.size());

	bazel_registry::metadata_config metadata =
		json::parse(std::ifstream{metadata_path});

	if(range_contains(metadata.versions, info.version)) {
		std::cerr << std::format(
			"version {} already added for {}\n",
			info.version,
			info.name
		);
		return 1;
	}

	metadata.versions.push_back(info.version);

	auto source = bazel_registry::source_config{};

	source.integrity = calc_sha256_integrity(FLAGS_archive).value();
	source.url = std::format(
		"https://github.com/bazelboost/{0}/releases/download/{1}/"
		"bazelboost-{0}-{1}.tar.gz",
		boost_module,
		info.version
	);

	auto source_json_path = module_dir / info.version / "source.json";
	fs::create_directories(source_json_path.parent_path(), ec);

	auto source_json = json{};
	to_json(source_json, source);

	auto source_file = std::ofstream{
		source_json_path,
		std::ios::trunc | std::ios::in | std::ios::out,
	};

	source_file << source_json.dump(2);
	source_file.flush();

	auto metadata_json = json{};
	to_json(metadata_json, metadata);

	auto metadata_config_file = std::ofstream{
		metadata_path,
		std::ios::trunc | std::ios::in | std::ios::out,
	};

	metadata_config_file << metadata_json.dump(2);
	metadata_config_file.flush();

	return 0;
}
