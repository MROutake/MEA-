#pragma once

/// @file Application.h
/// @brief Einziger Composition Root der Plattform (ADR 0001): instanziiert
///        HAL, Sensor, Prozessoren, Transport, Encoder, Sink, Manager und
///        Pipeline und verdrahtet sie über IDs.
///
/// Demo-Pipeline:
///   ESP32 ADC GPIO 34 -> AnalogInputSensor -> LinearProcessor (Rohwert->Volt)
///   -> ClampProcessor -> BufferedMeasurementSink -> CsvMeasurementEncoder
///   -> ArduinoStreamTransport -> Serial

#include <MeaAnalogInput.h>
#include <MeaCommunication.h>
#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaProcessing.h>
#include <MeaStateMachine.h>

#include "AppConfig.h"

namespace app {

class Application final {
public:
    Application() noexcept;

    /// Registriert Komponenten, initialisiert Manager (genau einmal, ADR 0004)
    /// und startet die Pipeline. Schwere Fehler werden über Serial gemeldet.
    void begin();

    /// Zyklischer Antrieb: Manager, Transport und Pipeline (nicht blockierend).
    void update(mea::TimestampMs nowMs);

private:
    static void reportStatus(const char* stage, const mea::Status& status);

    // HAL und Komponenten (statische Lebensdauer über die Application-Instanz).
    mea::ArduinoAnalogReader analogReader_;
    mea::AnalogInputSensor analogSensor_;
    mea::LinearProcessor rawToVoltage_;
    mea::ClampProcessor voltageClamp_;
    mea::ArduinoStreamTransport serialTransport_;
    mea::CsvMeasurementEncoder csvEncoder_;
    mea::BufferedMeasurementSink<config::kSinkQueueCapacity, config::kSinkFrameSize>
        serialSink_;

    // Manager (besitzen nichts, ADR 0001).
    mea::SensorManager<config::kManagerCapacity> sources_;
    mea::ProcessorManager<config::kManagerCapacity> processors_;
    mea::SinkManager<config::kManagerCapacity> sinks_;

    mea::MeasurementPipelineMachine pipeline_;

    mea::StatusCode lastReportedCode_{mea::StatusCode::Ok};
    bool healthy_{false};
};

}  // namespace app
