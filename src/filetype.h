#pragma once

#include <filesystem>
#include <ostream>

namespace packer {
// Enumerated type representing the type of a directory entry
enum class file_type : signed char {
    unknown = 0,
    regular = 1,
    duplicate = 2,
    directory = 3,
    leave_directory = 4,
    symlink = 5,
    block = 6,
    character = 7,
    fifo = 8,
    socket = 9,
};

inline file_type from_std_fs_type(const std::filesystem::file_type& ftype) {
    switch (ftype) {
        case std::filesystem::file_type::regular:
            return file_type::regular;
        case std::filesystem::file_type::directory:
            return file_type::directory;
        case std::filesystem::file_type::symlink:
            return file_type::symlink;
        case std::filesystem::file_type::block:
            return file_type::block;
        case std::filesystem::file_type::character:
            return file_type::character;
        case std::filesystem::file_type::fifo:
            return file_type::fifo;
        case std::filesystem::file_type::socket:
            return file_type::socket;
        case std::filesystem::file_type::none:
        case std::filesystem::file_type::not_found:
        case std::filesystem::file_type::unknown:
        default:
            return file_type::unknown;
    }
}

inline std::ostream& operator<<(std::ostream& os, const file_type& ft) {
    // output a text representation of the file_type
    switch (ft) {
        case file_type::unknown:
            os << "unknown";
            break;
        case file_type::regular:
            os << "regular";
            break;
        case file_type::duplicate:
            os << "duplicate";
            break;
        case file_type::directory:
            os << "directory";
            break;
        case file_type::leave_directory:
            os << "leave_directory";
            break;
        case file_type::symlink:
            os << "symlink";
            break;
        case file_type::block:
            os << "block";
            break;
        case file_type::character:
            os << "character";
            break;
        case file_type::fifo:
            os << "fifo";
            break;
        case file_type::socket:
            os << "socket";
            break;
        default:
            os << "invalid(" << static_cast<int>(ft) << ")";
            break;
    }
    return os;
}

} // namespace packer