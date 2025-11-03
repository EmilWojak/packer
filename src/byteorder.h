/*
 * @file endian.h
 * @brief Small cross-platform helpers to read/write little-endian integers.
 *
 * These helpers are intentionally tiny and header-only so they're easy to
 * include where needed. They implement a compile-time detection for
 * big-endian hosts and perform a byteswap only when necessary.
 *
 * Usage examples:
 *   write_le16(os, value);
 *   auto v = read_le16(is);
 */

#pragma once

#include <cstdint>
#include <istream>
#include <ostream>

namespace packer {

inline uint16_t bswap16(uint16_t x) noexcept {
    return static_cast<uint16_t>((x << 8) | (x >> 8));
}

inline uint32_t bswap32(uint32_t x) noexcept {
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) | ((x & 0x00FF0000u) >> 8) |
           ((x & 0xFF000000u) >> 24);
}

// Convert host -> little-endian (no-op on little-endian hosts)
inline uint16_t to_le16(uint16_t v) noexcept {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) ||                         \
    defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
    return bswap16(v);
#else
    return v;
#endif
}

inline uint32_t to_le32(uint32_t v) noexcept {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) ||                         \
    defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
    return bswap32(v);
#else
    return v;
#endif
}

inline uint64_t to_le64(uint64_t v) noexcept {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) ||                         \
    defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
    return (static_cast<uint64_t>(bswap32(static_cast<uint32_t>(v))) << 32) |
           static_cast<uint64_t>(bswap32(static_cast<uint32_t>(v >> 32)));
#else
    return v;
#endif
}

// Convert little-endian -> host (same as to_le on symmetric logic)
inline uint16_t from_le16(uint16_t v) noexcept {
    return to_le16(v);
}
inline uint32_t from_le32(uint32_t v) noexcept {
    return to_le32(v);
}
inline uint64_t from_le64(uint64_t v) noexcept {
    return to_le64(v);
}

// Read/write helpers that always serialize in little-endian byte order
inline void write_le16(std::ostream& os, uint16_t v) {
    uint16_t le = to_le16(v);
    os.write(reinterpret_cast<const char*>(&le), sizeof(le));
}

inline uint16_t read_le16(std::istream& is) {
    uint16_t le = 0;
    is.read(reinterpret_cast<char*>(&le), sizeof(le));
    return from_le16(le);
}

inline void write_le32(std::ostream& os, uint32_t v) {
    uint32_t le = to_le32(v);
    os.write(reinterpret_cast<const char*>(&le), sizeof(le));
}

inline uint32_t read_le32(std::istream& is) {
    uint32_t le = 0;
    is.read(reinterpret_cast<char*>(&le), sizeof(le));
    return from_le32(le);
}

inline void write_le64(std::ostream& os, uint64_t v) {
    uint64_t le = to_le64(v);
    os.write(reinterpret_cast<const char*>(&le), sizeof(le));
}

inline uint64_t read_le64(std::istream& is) {
    uint64_t le = 0;
    is.read(reinterpret_cast<char*>(&le), sizeof(le));
    return from_le64(le);
}

} // namespace packer
