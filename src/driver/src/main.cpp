#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <yaml-cpp/yaml.h>

#include <gennylib/version.hpp>

int main(int argc, char**argv) {
    // basically just a test that we're using c++17
    auto v { std::make_optional(genny::getVersion()) };
    std::cout << u8"🧞 Genny" << " Version " << v.value_or("ERROR") << u8" 💝🐹🌇⛔" << std::endl;

    if (argc <= 2) {
        return 0;
    }

    // test we can load yaml (just smoke-testing yaml for now, this will be real soon!)
    const char* fileName = argv[1];
    if (argc >= 3 && strncmp("--debug", argv[2], 100) != 0) {
        fileName = argv[2];
        auto yamlFile = std::ifstream {fileName, std::ios_base::binary};
        std::cout << "YAML File" << fileName << ":" << std::endl << yamlFile.rdbuf();
    }

    auto config = YAML::LoadFile(fileName);

    std::cout << std::endl << "Using Schema Version:" << std::endl
              << config["SchemaVersion"].as<std::string>() << std::endl;

    return 0;
}
