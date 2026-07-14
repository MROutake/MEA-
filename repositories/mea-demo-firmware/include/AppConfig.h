#pragma once

/// @file AppConfig.h
/// @brief Zentrale Anwendungs-Konfiguration: Abtastung, Verarbeitung,
///        Kommunikation und Pipeline-Verhalten an einem Ort.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>
#include <MeaStateMachine.h>

#include "BoardConfig.h"

namespace app::config {

// ---------------------------------------------------------------- Abtastung

/// Abstand zwischen zwei Messwerten des Sensors.
constexpr mea::TimestampMs kSensorSampleIntervalMs = 250;
/// Oversampling: Samples je Messwert (Mittelwert als Rohwert).
constexpr std::uint16_t kSamplesPerMeasurement = 8;
/// Obergrenze Samples je update()-Aufruf (nicht blockierend).
constexpr std::uint8_t kMaxSamplesPerUpdate = 2;

// ---------------------------------------------------------------- Verarbeitung

/// Lineare Umrechnung Rohwert -> Volt (vereinfachtes lineares ADC-Modell).
constexpr float kAdcToVoltGain =
    board::kAdcReferenceVolt / static_cast<float>(board::kAdcMaximumRaw);
constexpr float kVoltageOffset = 0.0F;

/// Gültiger Spannungsbereich für den ClampProcessor.
constexpr float kVoltageMin = 0.0F;
constexpr float kVoltageMax = board::kAdcReferenceVolt;

// ---------------------------------------------------------------- Kommunikation

/// Kapazität der Sink-Queue (Messwerte) und maximale CSV-Framelänge (Bytes).
constexpr std::size_t kSinkQueueCapacity = 8;
constexpr std::size_t kSinkFrameSize = 96;
constexpr std::uint8_t kCsvDecimalPlaces = 3;

// ---------------------------------------------------------------- Pipeline

constexpr mea::TimestampMs kPipelineCycleIntervalMs = 1000;
constexpr mea::TimestampMs kAcquisitionTimeoutMs = 2000;
constexpr mea::TimestampMs kPublishTimeoutMs = 500;
constexpr mea::RetryPolicy kRetryPolicy{250, 3};
constexpr bool kStartImmediately = true;

/// Kapazitäten der Manager (registrierbare Komponenten).
constexpr std::size_t kManagerCapacity = 4;

}  // namespace app::config
