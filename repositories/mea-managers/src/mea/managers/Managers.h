#pragma once

/// @file Managers.h
/// @brief Fachlich benannte Manager. Nur Manager mit aktualisierbaren
///        Komponenten besitzen updateAll().

#include <cstddef>

#include <MeaCore.h>

#include "ComponentManager.h"

namespace mea {

/// Verwaltet Messquellen (updateAll erforderlich).
template <std::size_t Capacity>
class SensorManager final
    : public UpdatableComponentManager<IMeasurementSource, Capacity> {};

/// Verwaltet Prozessoren (kein updateAll: Prozessoren sind passiv).
template <std::size_t Capacity>
class ProcessorManager final : public ComponentManager<IMeasurementProcessor, Capacity> {
};

/// Verwaltet Sinks (updateAll erforderlich: nicht blockierendes Nachschreiben).
template <std::size_t Capacity>
class SinkManager final : public UpdatableComponentManager<IMeasurementSink, Capacity> {};

/// Verwaltet Befehlsquellen (updateAll erforderlich).
template <std::size_t Capacity>
class CommandSourceManager final
    : public UpdatableComponentManager<ICommandSource, Capacity> {};

/// Verwaltet Befehls-Handler (kein updateAll: Handler sind passiv).
template <std::size_t Capacity>
class CommandHandlerManager final : public ComponentManager<ICommandHandler, Capacity> {};

}  // namespace mea
