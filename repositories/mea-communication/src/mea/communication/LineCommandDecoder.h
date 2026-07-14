#pragma once

/// @file LineCommandDecoder.h
/// @brief Vorbereitung eingehender Kommunikation (ADR 0006): kleiner,
///        zeilenorientierter Command-Decoder auf einem IByteTransport.
///
/// Zeilenformat (Version 1, Felder dezimal, Zeilenende '\n', '\r' wird ignoriert):
///
///     target_id;command_type;argument\n
///
/// Beispiel: "100;1;0\n" = Start an Komponente 100. Ungültige Zeilen werden
/// verworfen und über protocolErrors() gezählt; überlange Zeilen werden bis
/// zum nächsten Zeilenende ignoriert. Kein JSON, keine dynamische Allokation.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IByteTransport.h"

namespace mea {

class LineCommandDecoder final : public ICommandSource {
public:
    /// Maximale Zeilenlänge inklusive Zeilenende.
    static constexpr std::size_t kMaxLineLength = 32;
    /// Kapazität der Befehls-Queue.
    static constexpr std::size_t kQueueCapacity = 4;
    /// Obergrenze gelesener Bytes je update()-Aufruf (begrenzte Arbeit).
    static constexpr std::size_t kMaxBytesPerUpdate = 64;

    /// @param transport Nicht besitzend; muss den Decoder überleben (ADR 0001).
    LineCommandDecoder(IByteTransport& transport, const ComponentId decoderId) noexcept
        : transport_(transport), decoderId_(decoderId) {}

    [[nodiscard]] ComponentId id() const noexcept override { return decoderId_; }

    /// Reinitialisierend: leert Zeilenpuffer und Queue (ADR 0004).
    Status begin() noexcept override;

    Status update(TimestampMs nowMs) noexcept override;
    [[nodiscard]] std::size_t available() const noexcept override;
    Status read(Command& output) noexcept override;

    /// Diagnose: verworfene ungültige oder überlange Zeilen.
    [[nodiscard]] std::uint32_t protocolErrors() const noexcept {
        return protocolErrors_;
    }
    /// Diagnose: wegen voller Queue verworfene Befehle.
    [[nodiscard]] std::uint32_t droppedCommands() const noexcept {
        return droppedCommands_;
    }

private:
    void handleLine(TimestampMs nowMs) noexcept;
    [[nodiscard]] bool parseLine(Command& command) const noexcept;

    IByteTransport& transport_;
    ComponentId decoderId_;

    RingBuffer<Command, kQueueCapacity> queue_{};
    char line_[kMaxLineLength]{};
    std::size_t lineLength_{0};
    std::uint32_t protocolErrors_{0};
    std::uint32_t droppedCommands_{0};
    bool discardingLine_{false};
    bool initialized_{false};
};

}  // namespace mea
