#include "xxhasher.h"

#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>

namespace packer {

class XXH3StateGuard {
  public:
    XXH3StateGuard() : state(XXH3_createState()) {
        if (!state) {
            throw std::runtime_error("Failed to create XXH3 state");
        }
    }
    ~XXH3StateGuard() {
        if (state)
            XXH3_freeState(state);
    }
    XXH3_state_t* get() const { return state; }
    // Non-copyable
    XXH3StateGuard(const XXH3StateGuard&) = delete;
    XXH3StateGuard& operator=(const XXH3StateGuard&) = delete;

  private:
    XXH3_state_t* state;
};

StreamHasher::hash_value_t XXHasher::compute_hash(std::istream& input) const {
    XXH3StateGuard stateGuard;
    XXH3_state_t* state = stateGuard.get();
    if (state) {
        XXH3_64bits_reset(state);
    }
    constexpr std::size_t BUFFER_SIZE = 8192; // TODO: profile and tune buffer size
    char buffer[BUFFER_SIZE];
    while (input.read(buffer, sizeof(buffer))) {
        XXH3_64bits_update(state, buffer, input.gcount());
    }
    if (input.gcount() > 0) {
        XXH3_64bits_update(state, buffer, input.gcount());
    }

    hash_value_t result = XXH3_64bits_digest(state);
    return result;
}

} // namespace packer