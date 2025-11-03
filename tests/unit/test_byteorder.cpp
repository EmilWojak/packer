#include "byteorder.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

using namespace packer;

static std::string to_le_bytes(uint64_t v, size_t bytes) {
    std::string s;
    s.reserve(bytes);
    for (size_t i = 0; i < bytes; ++i) {
        s.push_back(static_cast<char>((v >> (8 * i)) & 0xFFu));
    }
    return s;
}

TEST(ByteOrder, BSwapBasic) {
    EXPECT_EQ(bswap16(0x1234u), 0x3412u);
    EXPECT_EQ(bswap16(0x0001u), 0x0100u);
    EXPECT_EQ(bswap32(0x01020304u), 0x04030201u);
    EXPECT_EQ(bswap32(0xAABBCCDDu), 0xDDCCBBAAu);
}

TEST(ByteOrder, ToFromLeRoundTrip) {
    const std::vector<uint16_t> vals16 = {0u, 1u, 0x1234u, 0xFFFFu};
    for (auto v : vals16) {
        EXPECT_EQ(from_le16(to_le16(v)), v);
    }

    const std::vector<uint32_t> vals32 = {0u, 1u, 0x01020304u, 0xFFFFFFFFu};
    for (auto v : vals32) {
        EXPECT_EQ(from_le32(to_le32(v)), v);
    }

    const std::vector<uint64_t> vals64 = {0ull, 1ull, 0x0102030405060708ull, 0xFFFFFFFFFFFFFFFFull};
    for (auto v : vals64) {
        EXPECT_EQ(from_le64(to_le64(v)), v);
    }
}

TEST(ByteOrder, WriteRead16_StreamRoundtrip) {
    std::ostringstream os(std::ios::binary);
    std::vector<uint16_t> written = {0x0000u, 0x1234u, 0xFFFFu};
    for (auto v : written)
        write_le16(os, v);

    std::istringstream is(os.str(), std::ios::binary);
    for (auto expected : written) {
        uint16_t got = read_le16(is);
        EXPECT_EQ(got, expected);
    }
    EXPECT_TRUE(is.eof() || is.peek() == std::char_traits<char>::eof());
}

TEST(ByteOrder, WriteLe32_ProducesLittleEndianBytes) {
    const uint32_t value = 0x11223344u;
    std::ostringstream os(std::ios::binary);
    write_le32(os, value);
    const std::string out = os.str();
    ASSERT_EQ(out.size(), sizeof(uint32_t));
    EXPECT_EQ(out, to_le_bytes(value, sizeof(uint32_t)));
}

TEST(ByteOrder, WriteRead64_StreamRoundtrip) {
    std::ostringstream os(std::ios::binary);
    std::vector<uint64_t> written = {0ull, 0x0102030405060708ull, 0xFFFFFFFFFFFFFFFFull};
    for (auto v : written)
        write_le64(os, v);

    std::istringstream is(os.str(), std::ios::binary);
    for (auto expected : written) {
        uint64_t got = read_le64(is);
        EXPECT_EQ(got, expected);
    }
    EXPECT_TRUE(is.eof() || is.peek() == std::char_traits<char>::eof());
}