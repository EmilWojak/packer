#pragma once

#include "streamhasher.h"

namespace packer {

class XXHasher : public StreamHasher {
  public:
    XXHasher() = default;
    ~XXHasher() override = default;

    hash_value_t compute_hash(std::istream& input) const override;
};

} // namespace packer