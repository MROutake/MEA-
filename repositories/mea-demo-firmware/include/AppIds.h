#pragma once

/// @file AppIds.h
/// @brief Zentrale Komponenten-IDs der Anwendung. Keine Magic Numbers im
///        Anwendungscode; ID 0 ist reserviert (mea::InvalidComponentId).

#include <MeaCore.h>

namespace app::ids {

constexpr mea::ComponentId AnalogInput1 = 100;
constexpr mea::ComponentId RawToVoltage = 200;
constexpr mea::ComponentId VoltageClamp = 201;
constexpr mea::ComponentId SerialOutput = 300;
constexpr mea::ComponentId MeasurementPipeline = 400;

}  // namespace app::ids
