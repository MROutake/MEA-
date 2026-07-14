#pragma once

/// @file Command.h
/// @brief Stabile Basistypen für spätere eingehende Kommunikation. Bewusst
///        minimal gehalten – kein universelles Nachrichtensystem.

#include <cstddef>
#include <cstdint>

#include "Interfaces.h"
#include "Status.h"
#include "Types.h"

namespace mea {

/// Art eines Befehls an eine Komponente.
enum class CommandType : std::uint16_t {
    Unknown = 0,
    Start,
    Stop,
    Reset,
    RequestMeasurement,
    SetParameter
};

/// Ein einzelner Befehl. Trivial kopierbar.
struct Command {
    ComponentId sourceId{InvalidComponentId};
    ComponentId targetId{InvalidComponentId};
    CommandType type{CommandType::Unknown};
    std::uint32_t argument{0};
    TimestampMs receivedAtMs{0};
};

/// Quelle eingehender Befehle (z. B. Kommando-Decoder auf einem Transport).
/// Semantik analog IMeasurementSource: update() nicht blockierend,
/// read() entnimmt genau einen Befehl (NoData, wenn keiner vorliegt).
class ICommandSource {
public:
    virtual ~ICommandSource() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;
    [[nodiscard]] virtual std::size_t available() const noexcept = 0;
    virtual Status read(Command& output) noexcept = 0;
};

/// Verarbeitet einen Befehl. handle() blockiert nicht.
class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status handle(const Command& command, TimestampMs nowMs) noexcept = 0;
};

using ICommandSourceLocator = IComponentLocator<ICommandSource>;
using ICommandHandlerLocator = IComponentLocator<ICommandHandler>;

}  // namespace mea
