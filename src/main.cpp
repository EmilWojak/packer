#include <filesystem>
#include <iostream>
#include <string>

#include "packer.h"
#include "xxhasher.h"
#include <cstring>

bool parse_arguments(int argc, char* argv[], bool& is_pack, std::filesystem::path& input_path,
                     std::filesystem::path& output_path) {
    if (argc != 4) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << argv[0] << " pack <input_path> <output_file>" << std::endl;
        std::cerr << "or" << std::endl;
        std::cerr << argv[0] << " unpack <input_file> <output_path>" << std::endl;
        return false;
    }

    std::string command = argv[1];
    if (command == "pack") {
        is_pack = true;
    } else if (command == "unpack") {
        is_pack = false;
    } else {
        std::cerr << "Invalid command: " << command << std::endl;
        return false;
    }
    input_path = std::filesystem::path(argv[2]);
    output_path = std::filesystem::path(argv[3]);
    return true;
}

int main(int argc, char* argv[]) {
    bool is_pack = false;
    std::filesystem::path input_path;
    std::filesystem::path output_path;

    if (!parse_arguments(argc, argv, is_pack, input_path, output_path)) {
        return 1;
    }

    packer::XXHasher hasher;
    packer::Packer packer{hasher};
    try {
        if (is_pack)
            packer.pack(input_path, output_path);
        else
            packer.unpack(input_path, output_path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}