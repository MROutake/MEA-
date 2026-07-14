#pragma once

/// @file Crc16.h
/// @brief CRC-16/CCITT-FALSE (Poly 0x1021, Init 0xFFFF, kein Reflect,
///        kein XorOut). Tabellenlos, ohne Allokation, für Frame-Integrität.

#include <cstddef>
#include <cstdint>

namespace mea {
namespace protocol {

/// Aktualisiert @p crc mit einem Byte.
[[nodiscard]] inline std::uint16_t crc16Update(std::uint16_t crc,
                                               const std::uint8_t byte) noexcept {
    crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(byte) << 8U));
    for (std::uint8_t bit = 0; bit < 8U; ++bit) {
        if ((crc & 0x8000U) != 0U) {
            crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
        } else {
            crc = static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return crc;
}

/// CRC-16/CCITT-FALSE über @p length Bytes ab @p data.
[[nodiscard]] inline std::uint16_t crc16(const std::uint8_t* data,
                                         const std::size_t length) noexcept {
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t index = 0; index < length; ++index) {
        crc = crc16Update(crc, data[index]);
    }
    return crc;
}

}  // namespace protocol
}  // namespace mea
