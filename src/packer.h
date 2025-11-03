#pragma once

#include "filetype.h"
#include "ifstream_exc.h"
#include "streamhasher.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace packer {

namespace fs = std::filesystem;

// Packer class for creating and extracting packed archives
class Packer {
  public:
    // constructor taking a path to the archive file
    Packer(const StreamHasher& stream_hasher);
    ~Packer();

    // method to create an archive from input path
    void pack(const fs::path& input_path, const fs::path& archive_path);
    // method to extract all entries from the archive
    void unpack(const fs::path& archive_path, const fs::path& output_path);

  private:
    static constexpr std::streamsize CHUNK_SIZE = 64 * 1024;

    // method to add an entry to the archive
    void add_entry(const fs::directory_entry& entry, int path_depth);

    std::streamoff getDuplicateFileOffset(const fs::path& file_path);
    std::streamoff findDuplicateFile(const fs::path& file_path,
                                     StreamHasher::hash_value_t& hash) const;
    bool filesAreIdentical(const fs::path& path1, const fs::path& path2) const;

    void writeLeaveDirectory(int depth_decrease);

    void writeMetadata(file_type file_type, const fs::path& file_path);
    bool extractMetadata(std::istream& archive_in, file_type& ft, fs::path& entry_name);

    void writePath(const fs::path& file_path);
    void extractPath(std::istream& archive_in, fs::path& out_path);

    void writeFileData(const fs::path& file_path);
    void extractFileData(std::istream& archive_in, const fs::path& out_path);

    const StreamHasher& hasher_;
    fs::path input_root_;
    int current_depth_ = 0;
    std::ofstream archive_file_;

    // store the mapping of file hashes to their original paths and offsets for duplicate detection
    typedef std::pair<fs::path, std::streamoff> PathOffsetPair;
    std::unordered_multimap<StreamHasher::hash_value_t, PathOffsetPair> file_hash_to_paths_;
};

} // namespace packer