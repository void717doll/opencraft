#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <mach-o/dyld.h>
#include <unistd.h>

int main(int, char **argv) {
	try {
		std::vector<char> executable(1024);
		uint32_t size = uint32_t(executable.size());
		if (_NSGetExecutablePath(executable.data(), &size) != 0) {
			executable.resize(size);
			if (_NSGetExecutablePath(executable.data(), &size) != 0)
				throw std::runtime_error("cannot locate app bundle");
		}
		namespace fs = std::filesystem;
		fs::path contents = fs::canonical(executable.data()).parent_path().parent_path();
		fs::path assets = contents / "Resources/opencraft";
		fs::path data;
		const char *overridePath = std::getenv("OPENCRAFT_DATA_DIR");
		if (overridePath && *overridePath) data = overridePath;
		else {
			const char *homePath = std::getenv("HOME");
			if (!homePath || !*homePath) throw std::runtime_error("home directory is unavailable");
			data = fs::path(homePath) / "Library/Application Support/OpenCraft";
			fs::path legacy = fs::path(homePath) / "Library/Application Support/OpenMiner";
			if (!fs::exists(data) && fs::is_directory(legacy)) fs::rename(legacy, data);
		}
		for (const char *directory : {"config", "saves", "logs"}) fs::create_directories(data / directory);
		for (const char *directory : {"mods", "resources"}) {
			fs::path link = data / directory;
			if (fs::is_symlink(link)) fs::remove(link);
			if (!fs::exists(link)) fs::create_directory_symlink(assets / directory, link);
		}
		fs::current_path(data);
		std::string binary = (contents / "MacOS/opencraft-bin").string();
		argv[0] = binary.data();
		execv(binary.c_str(), argv);
		std::perror("cannot launch opencraft");
	} catch (const std::exception &error) {
		std::fprintf(stderr, "cannot launch opencraft: %s\n", error.what());
	}
	return 1;
}
