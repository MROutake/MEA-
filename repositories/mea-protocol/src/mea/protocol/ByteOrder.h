#pragma once

/// @file ByteOrder.h
/// @brief Explizite Little-Endian-(De)Serialisierung in/aus Aufruferpuffern.
///        Kein struct-Packing über die Leitung: reproduzierbare Golden Frames,
///        keine Alignment-/Endianness-Abhängigkeit, -Wconversion-sauber.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mea {
namespace protocol {
namespace le {

/// Schreibt ein Byte und rückt den Offset vor.
inline void writeU8(std::uint8_t* buffer, std::size_t& offset,
                    const std::uint8_t value) noexcept {
    buffer[offset] = value;
    offset += 1U;
}

/// Schreibt einen u16 little-endian.
inline void writeU16(std::uint8_t* buffer, std::size_t& offset,
                     const std::uint16_t value) noexcept {
    buffer[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    buffer[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    offset += 2U;
}

/// Schreibt einen u32 little-endian.
inline void writeU32(std::uint8_t* buffer, std::size_t& offset,
                     const std::uint32_t value) noexcept {
    buffer[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    buffer[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    buffer[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    buffer[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    offset += 4U;
}

/// Schreibt einen IEEE-754 float32 little-endian (Bitmuster via memcpy).
inline void writeF32(std::uint8_t* buffer, std::size_t& offset,
                     const float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(buffer, offset, bits);
}

/// Liest ein Byte und rückt den Offset vor.
[[nodiscard]] inline std::uint8_t readU8(const std::uint8_t* buffer,
                                         std::size_t& offset) noexcept {
    const std::uint8_t value = buffer[offset];
    offset += 1U;
    return value;
}

/// Liest einen u16 little-endian.
[[nodiscard]] inline std::uint16_t readU16(const std::uint8_t* buffer,
                                           std::size_t& offset) noexcept {
    const std::uint16_t low = buffer[offset];
    const std::uint16_t high = buffer[offset + 1U];
    offset += 2U;
    return static_cast<std::uint16_t>(low | (high << 8U));
}

/// Liest einen u32 little-endian.
[[nodiscard]] inline std::uint32_t readU32(const std::uint8_t* buffer,
                                           std::size_t& offset) noexcept {
    const std::uint32_t b0 = buffer[offset];
    const std::uint32_t b1 = buffer[offset + 1U];
    const std::uint32_t b2 = buffer[offset + 2U];
    const std::uint32_t b3 = buffer[offset + 3U];
    offset += 4U;
    return b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
}

/// Liest einen IEEE-754 float32 little-endian.
[[nodiscard]] inline float readF32(const std::uint8_t* buffer,
                                   std::size_t& offset) noexcept {
    const std::uint32_t bits = readU32(buffer, offset);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace le
}  // namespace protocol
}  // namespace mea
