#include "packer.h"

#include "byteorder.h"
#include "filetype.h"
#include "ifstream_exc.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace packer {

Packer::Packer(const StreamHasher& stream_hasher) : hasher_(stream_hasher) {
    archive_file_.exceptions(std::ios::failbit | std::ios::badbit);
}

Packer::~Packer() {
    if (archive_file_.is_open()) {
        archive_file_.close();
    }
}

// Archive format per entry:
// Metadata: [1 byte: file type][2 bytes:: path length][path bytes]
// Followed by content depending on file type:
// For regular files: [4 bytes: data length][file content bytes]
// For duplicate files: [8 bytes: offset of original file data]
// For symlinks: [2 bytes: target path length][target path bytes]
// when leaving directories:
// [1 byte: file_type::leave_directory][2 bytes: depth decrease]
void Packer::pack(const fs::path& input_path, const fs::path& archive_path) {
    input_root_ = input_path;
    archive_file_.open(archive_path, std::ios::binary);
    this->current_depth_ = 0;
    this->file_hash_to_paths_.clear();

    for (auto it = fs::recursive_directory_iterator(input_path, fs::directory_options::none);
         it != fs::recursive_directory_iterator(); ++it) {

        const fs::directory_entry& entry = *it;
        std::streamoff entry_offset = 0;
        try {
            entry_offset = archive_file_.tellp();
            add_entry(entry, it.depth());
        } catch (const std::runtime_error& e) {
            archive_file_.seekp(entry_offset); // rollback to before entry
            std::cerr << "Error packing entry " << entry.path() << ": " << e.what() << std::endl;
            continue;
        }
    }
}

void Packer::unpack(const fs::path& archive_path, const fs::path& output_path) {
    // open archive for reading
    ifstream_exc archive_in(archive_path, std::ios::binary);

    fs::path current_directory = output_path;

    file_type ft;
    fs::path entry_name;
    while (extractMetadata(archive_in, ft, entry_name)) {
        fs::path full_entry_path = current_directory / entry_name;
        std::cout << "File path: " << full_entry_path << std::endl;

        switch (ft) {
            case file_type::directory: {
                // push directory component and create it
                fs::create_directories(full_entry_path);
                std::cout << "Created directory: " << full_entry_path << std::endl;
                current_directory = full_entry_path;
                break;
            }
            case file_type::leave_directory: {
                // read depth decrease
                std::uint16_t depth_decrease = packer::read_le16(archive_in);
                if (depth_decrease == 0) {
                    throw std::runtime_error(
                        "Archive format error: zero depth decrease on leave_directory");
                }
                while (depth_decrease-- > 0) {
                    if (current_directory == output_path) {
                        throw std::runtime_error(
                            "Archive format error: attempt to leave root directory");
                    }
                    current_directory = current_directory.parent_path();
                }
                std::cout << "Moved up to directory: " << current_directory << std::endl;
                break;
            }
            case file_type::regular: {
                extractFileData(archive_in, full_entry_path);
                std::cout << "Extracted regular file: " << full_entry_path << std::endl;
                break;
            }
            case file_type::duplicate: {
                // read offset of original file (where its 4-byte length is stored)
                const std::streamoff orig_offset = packer::read_le64(archive_in);
                // remember current position to return after copying
                const std::streampos resume_pos = archive_in.tellg();
                // seek to original file data
                archive_in.seekg(orig_offset);

                extractFileData(archive_in, full_entry_path);

                // restore read position to continue processing
                archive_in.seekg(resume_pos);
                std::cout << "Created duplicate file from offset " << orig_offset << std::endl;
                break;
            }
            case file_type::symlink: {
                // symlink target is stored as a path (writePath)
                fs::path target;
                extractPath(archive_in, target);
                std::error_code ec;
                fs::create_symlink(target, full_entry_path, ec);
                if (ec) {
                    // try directory-symlink as fallback (platform dependent)
                    fs::create_directory_symlink(target, full_entry_path, ec);
                    if (ec) {
                        throw std::runtime_error("Failed to create symlink \"" +
                                                 full_entry_path.string() + "\" to \"" +
                                                 target.string() + "\": " + ec.message());
                    }
                }
                std::cout << "Created symlink: " << full_entry_path << " -> " << target
                          << std::endl;
                break;
            }
            default:
                throw std::runtime_error("Unsupported file type in archive: " +
                                         std::to_string(static_cast<int>(ft)));
        }
    }
}

// Add an entry to the archive
void Packer::add_entry(const fs::directory_entry& entry, int path_depth) {
    constexpr std::size_t MAX_PATH_SIZE = std::numeric_limits<std::uint16_t>::max();

    file_type file_type = from_std_fs_type(entry.symlink_status().type());
    const fs::path relative_path = fs::relative(entry.path(), input_root_);

    // handle directory depth decreases
    if (path_depth != current_depth_) {
        int depth_decrease = current_depth_ - path_depth;
        writeLeaveDirectory(depth_decrease);
        current_depth_ = path_depth;
    }

    // for regular files, check for duplicates
    std::streamoff duplicate_offset = 0;
    if (file_type == file_type::regular) {
        duplicate_offset = getDuplicateFileOffset(entry.path());
        if (duplicate_offset != 0) {
            file_type = file_type::duplicate;
        }
    }
    writeMetadata(file_type, entry.path().filename());

    switch (file_type) {
        case file_type::regular:
            writeFileData(entry.path());
            break;
        case file_type::duplicate:
            // write the offset of the original file
            write_le64(archive_file_, duplicate_offset);
            break;
        case file_type::symlink:
            // write the symlink target path
            writePath(fs::read_symlink(entry.path()));
            break;
        case file_type::directory:
            ++current_depth_;
            // nothing else to write for directories
            break;
        default:
            throw std::runtime_error("Unsupported file type " +
                                     std::to_string(static_cast<int>(file_type)) +
                                     " for packing: " + entry.path().string());
    }

    archive_file_.flush();
}

std::streamoff Packer::getDuplicateFileOffset(const fs::path& file_path) {
    // compute hash of the file
    StreamHasher::hash_value_t hash = 0;
    // nested scope to ensure ifstream is closed before further processing
    {
        packer::ifstream_exc input_file(file_path, std::ios::binary);
        hash = hasher_.compute_hash(input_file);
    }
    // check for duplicate by hash and content
    std::streamoff duplicate_offset = findDuplicateFile(file_path, hash);
    if (duplicate_offset == 0) {
        // not a duplicate, store hash and path with offset to file content
        auto content_offset = archive_file_.tellp();
        // skip metadata size to get to content offset
        content_offset += sizeof(std::uint8_t) +                // file type
                          sizeof(std::uint16_t) +               // path length
                          file_path.filename().string().size(); // path bytes

        file_hash_to_paths_.emplace(hash, std::make_pair(file_path, content_offset));
    }

    return duplicate_offset;
}

// check for duplicate files by hash and content
// returns a non-zero offset of the original file content if found, 0 otherwise
std::streamoff Packer::findDuplicateFile(const fs::path& file_path,
                                         StreamHasher::hash_value_t& hash) const {
    // check if we have seen this hash before
    const auto range = file_hash_to_paths_.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it) {
        // check if files are actually identical (hash collision possible)
        const fs::path& same_hash_path = it->second.first;
        if (filesAreIdentical(file_path, same_hash_path)) {
            std::streamoff duplicate_offset = it->second.second;
            return duplicate_offset; // found duplicate
        }
    }
    // offset is guaranteed to be greater than 0 for actual duplicates
    // because it points to file content after metadata
    return 0; // no duplicate
}

// read and compare both files in chunks
bool Packer::filesAreIdentical(const fs::path& path1, const fs::path& path2) const {
    std::vector<char> buf1(CHUNK_SIZE);
    std::vector<char> buf2(CHUNK_SIZE);
    packer::ifstream_exc ifs1(path1, std::ios::binary);
    packer::ifstream_exc ifs2(path2, std::ios::binary);
    while (ifs1 && ifs2) {
        ifs1.read(buf1.data(), CHUNK_SIZE);
        ifs2.read(buf2.data(), CHUNK_SIZE);
        std::streamsize bytes_read1 = ifs1.gcount();
        std::streamsize bytes_read2 = ifs2.gcount();
        if (bytes_read1 != bytes_read2 ||
            std::memcmp(buf1.data(), buf2.data(), static_cast<std::size_t>(bytes_read1)) != 0) {
            return false; // files differ
        }
    }
    return true; // files are identical
}

void Packer::writeLeaveDirectory(int depth_decrease) {
    // write the file_type::leave_directory value (cast to a byte)
    std::uint8_t type_byte = static_cast<std::uint8_t>(file_type::leave_directory);
    archive_file_.write(reinterpret_cast<const char*>(&type_byte), sizeof(type_byte));
    // write the depth decrease as 16 bits little-endian
    write_le16(archive_file_, static_cast<std::uint16_t>(depth_decrease));
}

void Packer::writeMetadata(file_type file_type, const fs::path& file_path) {
    constexpr std::size_t MAX_PATH_SIZE = std::numeric_limits<std::uint16_t>::max();

    // write the file_type value (cast to a byte)
    std::uint8_t type_byte = static_cast<std::uint8_t>(file_type);
    archive_file_.write(reinterpret_cast<const char*>(&type_byte), sizeof(type_byte));

    writePath(file_path);
}

bool Packer::extractMetadata(std::istream& archive_in, file_type& ft, fs::path& entry_name) {
    // read file_type byte
    std::uint8_t type_byte = 0;
    archive_in.read(reinterpret_cast<char*>(&type_byte), sizeof(type_byte));
    if (archive_in.eof()) {
        return false; // reached end of archive
    }
    if (archive_in.gcount() != sizeof(type_byte)) {
        throw std::runtime_error("Unexpected EOF while reading file type from archive");
    }
    ft = static_cast<file_type>(type_byte);

    if (ft != file_type::leave_directory) {
        // read filename (path fragment) written by writePath
        extractPath(archive_in, entry_name);
        if (entry_name.empty()) {
            throw std::runtime_error("Archive format error: empty file name");
        }
    }
    return true;
}

// write a file path to the archive
void Packer::writePath(const fs::path& file_path) {
    constexpr std::size_t MAX_PATH_SIZE = std::numeric_limits<std::uint16_t>::max();

    const std::string file_path_str = file_path.string();
    if (file_path_str.size() > MAX_PATH_SIZE) {
        throw std::range_error("Path too long to store in archive: " + file_path.string());
    }
    std::uint16_t path_length = static_cast<std::uint16_t>(file_path_str.size());
    // write length of path on 16 bits little-endian
    write_le16(archive_file_, path_length);
    // write path bytes
    archive_file_.write(file_path_str.data(), file_path_str.size());
}

void Packer::extractPath(std::istream& archive_in, fs::path& out_path) {
    // read length of path
    std::streamsize path_length = read_le16(archive_in);
    // read path bytes
    std::string path_str;
    path_str.resize(path_length);
    archive_in.read(path_str.data(), path_length);
    if (archive_in.gcount() != path_length) {
        throw std::runtime_error("Unexpected EOF while reading path from archive");
    }
    out_path = fs::path(path_str);
}

// write the contents of a regular file to the archive
void Packer::writeFileData(const fs::path& file_path) {
    constexpr std::size_t MAX_FILE_SIZE = std::numeric_limits<std::uint32_t>::max();

    auto file_size = fs::file_size(file_path);
    // check for file size not fitting 32 bits
    if (file_size > MAX_FILE_SIZE) {
        throw std::range_error("File of size " + std::to_string(file_size) +
                               " too large to store in archive: " + file_path.string());
    }
    std::uint32_t data_len = static_cast<std::uint32_t>(file_size);
    packer::write_le32(archive_file_, data_len);

    // stream file contents into the archive (if any)
    if (data_len > 0) {
        packer::ifstream_exc input_file(file_path, std::ios::binary);

        std::vector<char> buf(CHUNK_SIZE);
        std::streamsize remaining = data_len;
        while (remaining > 0) {
            std::streamsize to_read = std::min(remaining, CHUNK_SIZE);
            input_file.read(buf.data(), to_read);
            if (input_file.gcount() != to_read) {
                throw std::runtime_error("Unexpected EOF while reading file: " +
                                         file_path.string());
            }
            archive_file_.write(buf.data(), to_read);
            remaining -= to_read;
        }
    }
}

void Packer::extractFileData(std::istream& archive_in, const fs::path& out_path) {
    std::ofstream out;
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out.open(out_path, std::ios::binary);

    // read length of file data
    const std::uint32_t data_len = packer::read_le32(archive_in);
    // read file data in chunks
    std::vector<char> buf(CHUNK_SIZE);
    std::streamsize remaining = data_len;
    while (remaining > 0) {
        std::streamsize to_read = std::min(remaining, CHUNK_SIZE);
        archive_in.read(buf.data(), to_read);
        if (archive_in.gcount() != to_read) {
            throw std::runtime_error("Unexpected EOF while extracting file: " + out_path.string());
        }
        out.write(buf.data(), to_read);
        remaining -= to_read;
    }
    out.close();
}

} // namespace packer