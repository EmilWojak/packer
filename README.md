# Packer

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=c%2B%2B&logoColor=white) ](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-%3E%3D3.14-blue?logo=cmake&logoColor=white)](https://cmake.org/)
[![CTest](https://img.shields.io/badge/CTest-enabled-brightgreen?logo=cmake&logoColor=white)](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
[![GoogleTest](https://img.shields.io/badge/GoogleTest-enabled-4285F4?logo=google&logoColor=white)](https://github.com/google/googletest)
[![pytest](https://img.shields.io/badge/pytest-enabled-4B8BBE?logo=pytest&logoColor=white)](https://docs.pytest.org/)

A command-line tool to pack a folder into a compact archive file and to unpack it back into a folder.

## Summary

This repository contains a small, portable command-line *packer* utility that recursively traverses a filesystem tree and stores archive entries in a compact binary format of its own. The packing algorithms detects files with identical contents and stores such content only once per archive.

The project:
- is written in *C++-17*
- compiles with *GCC* 13.3.0 on Ubuntu 24.04
- builds with *CMake*>=3.14
- uses *GoogleTest* for unit tests
- uses *pytest* that works with *Python*>=3.12 for integration tests.

## Table of contents
- [Quick start — build and run tests](#quick-start--build-and-run-tests)
- [Project layout](#project-layout)
- [Archive format](#archive-format)
- [TODO](#todo)
- [Contributing and tests](#contributing-and-tests)

## Quick start — build and run tests

Install build-time, test-time and run-time dependencies from [dependencies.txt](dependencies.txt)

```bash
# configure, build and run tests
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Or use the convenience script:

```bash
./run-tests.sh
```

Run the program (after building)

```bash
# pack an existing directory into an archive file
./build/src/packer pack <input-directory> <archive-file>
# unpack an archive into an existing (ideally empty) directory
./build/src/packer unpack <archive-file> <output-directory>
```

## Project layout
- [src](src/) — application sources
- [tests](tests/) — unit and integration tests (GoogleTest + pytest-based integration)

## Archive format

### Format requirements and design decisions
- size of archive
    
    Supporting design choices:
    - detection of files with identical contents and storing such contents only once per archive,
    - storing file or directory name only, without the relative path and relying on a sequence of _directory_ and _leave directory_ entries to describe directory structure,
    - using variable length blocks prepended with block length to store sequences of bytes (e.g. entry names or file data),
    - no other attributes are stored (e.g. no permissions nor timesmaps).

- performance of packing

    Supporting design choices:
    - lack of archive index - entries are stored serially by appending to the archive, hence no support for amending entries in an already existing archive,
    - hashing is performed with the `XX3_64bit` algorithm from [xxHash](https://xxhash.com) - a high performace hashing library that advertises itself as:
        > an extremely fast non-cryptographic hash algorithm, working at RAM speed limit. It is proposed in four flavors (XXH32, XXH64, XXH3_64bits and XXH3_128bits). The latest variant, XXH3, offers improved performance across the board, especially on small data.


Each directory entry (e.g. a file, a subdirectory or a symbolic link) is stored as a single entry block in the following format:
- Metadata (always starts an entry):
    - 1 byte: file type (one byte value)
    - If file type != leave_directory:
        - 2 bytes: entry name length (uint16)
        - N bytes: entry name (without leading path)
- entry data (specific to entry type).

Following metadata the entry contains type-specific data:

- Regular file
    - 4 bytes: data length (uint32) — number of file bytes
    - data bytes: file content (data length bytes)

- Duplicate file
    - 8 bytes: offset of original file data (uint64)
        
    The offset is a file position inside the archive that points to the original file's data length field (the 4‑byte uint32 that precedes the original file content). When npacking, the reader seeks to this offset and reads the original file's length + content to recreate the duplicate.

- Symlink
    - 2 bytes: target path length (uint16)
    - M bytes: target path bytes (UTF‑8)

- Directory
    
    No extra payload after the metadata. All subsequent entries other than `leave_directory` are unpacked into this directory unless superceded by another _directory_ or _leave directory_ entry.

- Leave directory
    - 2 bytes: depth decrease (uint16) — no path bytes follow

    This instructs the unpacker to move up the directory tree by that number of levels and continue unpacking entries in the higher level folder.

    **Note**: an empty directory entry will be immediately followed by a _leave directory_ entry.

### Limits and notes
- Entry name length is stored as a 16‑bit unsigned integer - maximum name length is 65535 bytes. Entries with longer names are skipped.
- Regular file size is stored as a 32‑bit unsigned integer - maximum storable file size is 4294967295 bytes. Larger files are skipped.
- Any type of error when adding an entry to the archive will result in an error message printed to standard error and archive will be rewound back to the and of the previous entry to avoid archive format corruption.
- Duplicate detection during packing uses a hash value computed from the file's contents and a byte‑wise comparison to ensure identical contents in case of a hash collosion. When a duplicate is found, the archive stores a duplicate record that references the original content's offset instead of repeating the bytes.
- Directory entries are emitted when descending into a subdirectory; leave_directory entries are written when the traversal moves up by one or more levels at once. The archive therefore represents the tree structure as a sequence of push(directory) and pop(n) (leave_directory) operations together with files/symlink entries.
- All offsets are byte offsets relative to the start of the archive file.
- All offsets and lengths are stored in little‑endian byte order regardless of the machine's architecture.

#### A sample entry for a regular file "foo.txt" containing the word "bar":
- [1 byte: file_type = 1 (regular) `[0x01]`]
- [2 bytes: name_len = 7 `[0x07 0x00]`]
- [7 bytes: file_name = "foo.txt" `[0x66 0x6f 0x6f 0x2e 0x74 0x78 0x74]`]
- [4 bytes: data_len = 3 `[0x03 0x00 0x00 0x00]`]
- [3 bytes: file_data = "bar" `[0x62 0x61 0x72]`]

#### A sample entry for a duplicate "bar.txt" referencing earlier "foo.txt":
- [1 byte: file_type = 2 (duplicate) `[0x02]`]
- [2 bytes: name_len = 7 `[0x07 0x00]`]
- [7 bytes: file_name = "bar.txt" `[0x62 0x61 0x72 0x2e 0x74 0x78 0x74]`]
- [8 bytes: offset → points to the 4‑byte data_len of "foo.txt", e.g. offset=3 considering the previous example above as the first entry in the archive `[0x03 0x00 0x00 0x00 0x00 0x00 0x00 0x00]`]

#### A sample entry for a directory named "subfolder"
- [1 byte: file_type = 3 (directory) `[0x03]`]
- [2 bytes: name_len = 9 `[0x09 0x00]`]
- [9 bytes: file_name = "subfolder" `[0x73 0x75 0x62 0x66 0x6f 0x6c 0x64 0x65 0x72]`]

#### A sample entry for a leave_directory entry to go 2 levels up
- [1 byte: file_type = 4 (leave_directory) `[0x04]`]
- [2 bytes: depth_decrease = 2 `[0x02 0x00]`]

#### A sample entry for a symbolic link named "input_data.txt" pointing to "../document.txt"
- [1 byte: file_type = 5 (symlink) `[0x05]`]
- [2 bytes: name_len = 14 `[0x0e 0x00]`]
- [14 bytes: file_name = "input_data.txt" `[0x69 0x6e 0x70 0x75 0x74 0x5f 0x64 0x61 0x74 0x61 0x2e 0x74 0x78 0x74]`]
- [2 bytes: target_len = 15 `[0x0f 0x00]`]
- [15 bytes: target_path = "../document.txt" `[0x2e 0x2e 0x2f 0x64 0x6f 0x63 0x75 0x6d 0x65 0x6e 0x74 0x2e 0x74 0x78 0x74]`]

This layout lets the unpacker stream the archive, recreate directories, restore symlinks, write files and to write duplicate files by copying from the original file content region referenced by offsets.

## TODO
* use a compression library (zlib?) to either:
    - compress the whole archive

        This may require format change for storing duplicates which now rely on offsets into the archive file. Could use file path instead and copy an already unpacked original file when unpacking.
    - or to compress individual files' contents only.
    
    Subject to performance evaluation.

* dockerize the project,
* add unit tests for Packer
* add more comprehensive integration tests covering all sorts of errors, e.g. filesystem access errors or corrupted archive format when unpacking,
* profile and tune buffer sizes when reading files or hashing data
* experiment with another approach to appending data
    
    Current packing implementation reads each input file up to 3 times:
    * once for hashing
    * once for comparison if a hash match is found
    * once to copy file's data into the archive when no duplicate is found
    Try reading the input file once to perform hashing and appending data in a single loop through file data chunks; read the file once more for full content comparison; rewind the output archive offset should a duplicate match occur. Outcome would be subject to input data characteristics, e.g. file sizes and frequency of duplicates.

* support other file types:
    - character and block devices
    - named pipes
    - named sockets
* distinguish between duplicate files versus hardlinks
* test and document portability across various compilers, architectures and operating systems

## Contributing and tests
- Add unit tests under `tests/unit` and integration tests under `tests/integration`.
- Run `./run-tests.sh` to execute the full test suite.
