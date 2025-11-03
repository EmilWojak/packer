#pragma once

#include <fstream>
#include <ios>
#include <string>

namespace packer {

class ifstream_exc : public std::ifstream {
  public:
    ifstream_exc() {
        // enable exceptions for badbit
        this->exceptions(std::ios::badbit);
    }

    explicit ifstream_exc(const std::string& path, std::ios_base::openmode mode = std::ios::in)
        : ifstream_exc() // delegate to default ctor which sets exceptions
    {
        this->open(path, mode); // exceptions already set
    }

    // inherit is_open/close from std::ifstream; keep special member semantics
    ifstream_exc(const ifstream_exc&) = delete;
    ifstream_exc& operator=(const ifstream_exc&) = delete;
    ifstream_exc(ifstream_exc&&) = default;
    ifstream_exc& operator=(ifstream_exc&&) = default;
};

} // namespace packer