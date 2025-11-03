#pragma once

#include "xxhash.h"
#include <array>
#include <cstdint>
#include <istream>

namespace packer {

class StreamHasher {
  public:
    // Alias for hash values returned by StreamHasher implementations
    using hash_value_t = std::uint64_t;

    virtual ~StreamHasher() = default;
    // Compute hash of stream content in one call
    virtual hash_value_t compute_hash(std::istream& input) const = 0;
};

} // namespace packer
