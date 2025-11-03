#include "xxhasher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <xxhash.h>

#include <sstream>
#include <string>
#include <vector>

using namespace packer;
using hash_value_t = StreamHasher::hash_value_t;

using ::testing::Eq;
using ::testing::Not;

TEST(XXHasherTest, EmptyStreamProducesKnownHash) {
    XXHasher hasher;
    std::istringstream input("");
    hash_value_t result = hasher.compute_hash(input);

    // XXH3_64bits of empty input with seed 0 is a known constant
    const hash_value_t expected = static_cast<hash_value_t>(XXH3_64bits("", 0));
    EXPECT_EQ(result, expected);
}

TEST(XXHasherTest, SameContentProducesSameHash) {
    XXHasher hasher;
    const std::string data = "The quick brown fox jumps over the lazy dog";

    std::istringstream a(data);
    std::istringstream b(data);

    hash_value_t ha = hasher.compute_hash(a);
    hash_value_t hb = hasher.compute_hash(b);

    EXPECT_EQ(ha, hb);

    // and equals direct XXH3_64bits result
    const hash_value_t expected = static_cast<hash_value_t>(XXH3_64bits(data.data(), data.size()));
    EXPECT_EQ(ha, expected);
}

TEST(XXHasherTest, DifferentContentProducesDifferentHash) {
    XXHasher hasher;
    std::istringstream a("hello");
    std::istringstream b("world");

    hash_value_t ha = hasher.compute_hash(a);
    hash_value_t hb = hasher.compute_hash(b);

    EXPECT_NE(ha, hb);
}

TEST(XXHasherTest, HandlesExactChunkSize) {
    XXHasher hasher;
    // buffer size used in implementation is 4096
    const std::size_t chunk = 4096;
    std::string data(chunk, 'x');

    std::istringstream input(data);
    hash_value_t result = hasher.compute_hash(input);

    const hash_value_t expected = static_cast<hash_value_t>(XXH3_64bits(data.data(), data.size()));
    EXPECT_EQ(result, expected);
}

TEST(XXHasherTest, HandlesMultiChunkInput) {
    XXHasher hasher;
    // produce input larger than one chunk to exercise multiple reads
    const std::size_t size = 4096 * 3 + 123; // multiple full chunks + partial
    std::string data;
    data.reserve(size);
    for (std::size_t i = 0; i < size; ++i)
        data.push_back(static_cast<char>('A' + (i % 26)));

    std::istringstream input(data);
    hash_value_t result = hasher.compute_hash(input);

    const hash_value_t expected = static_cast<hash_value_t>(XXH3_64bits(data.data(), data.size()));
    EXPECT_EQ(result, expected);
}