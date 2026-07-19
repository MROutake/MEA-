#pragma once

/// @file IByteTransport.h
/// @brief Nicht blockierender Byte-Transport (ADR 0006, Schicht 1).
///        Lebenszyklus (begin/update aus IDevice): Composition Root oder
///        Runtime rufen begin() und update() auf; Sinks/Decoder benutzen ihn nur.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

namespace mea {

class IByteTransport : public IDevice {
public:

    /// Anzahl Bytes, die write() aktuell ohne Blockieren annimmt (darf 0 sein).
    [[nodiscard]] virtual std::size_t writable() const noexcept = 0;

    /// Schreibt höchstens @p size Bytes, ohne zu blockieren. @p written meldet
    /// die tatsächlich übernommene Anzahl (partielle Writes sind normal und
    /// kein Fehler).
    virtual Status write(const std::uint8_t* data, std::size_t size,
                         std::size_t& written) noexcept = 0;

    /// Anzahl lesbarer Bytes.
    [[nodiscard]] virtual std::size_t readable() const noexcept = 0;

    /// Liest höchstens @p capacity Bytes, ohne zu blockieren.
    virtual Status read(std::uint8_t* data, std::size_t capacity,
                        std::size_t& readCount) noexcept = 0;
};

}  // namespace mea
