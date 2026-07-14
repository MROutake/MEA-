#pragma once

/// @file ContractChecks.h
/// @brief Wiederverwendbare Contract-Tests für die Kerninterfaces. Nur für
///        Testcode gedacht; unabhängig vom Test-Framework (liefert das erste
///        verletzte Vertragsdetail als Text, nullptr = Vertrag erfüllt).
///
/// Die Checks prüfen den Vertrag einer frisch konstruierten, noch nicht
/// initialisierten Komponente (ADR 0004: vor begin() gilt NotInitialized).

#include <cstddef>

#include "../core/Interfaces.h"
#include "../core/Measurement.h"
#include "../core/Status.h"

namespace mea::testing {

/// Vertrag einer Source vor begin(): NotInitialized, keine Daten.
[[nodiscard]] inline const char* checkSourcePreBeginContract(
    IMeasurementSource& source) noexcept {
    if (source.id() == InvalidComponentId) {
        return "Source: id() darf nicht InvalidComponentId sein";
    }
    if (source.update(0).code != StatusCode::NotInitialized) {
        return "Source: update() vor begin() muss NotInitialized melden";
    }
    if (source.available() != 0) {
        return "Source: available() vor begin() muss 0 sein";
    }
    Measurement out{};
    if (source.read(out).code != StatusCode::NotInitialized) {
        return "Source: read() vor begin() muss NotInitialized melden";
    }
    return nullptr;
}

/// Vertrag einer initialisierten Source ohne Daten: read() liefert NoData.
[[nodiscard]] inline const char* checkSourceEmptyReadContract(
    IMeasurementSource& source) noexcept {
    if (source.available() != 0) {
        return nullptr;  // Quelle hat Daten; Leer-Vertrag nicht anwendbar.
    }
    Measurement out{};
    if (source.read(out).code != StatusCode::NoData) {
        return "Source: read() ohne verfügbare Daten muss NoData melden";
    }
    return nullptr;
}

/// Vertrag eines Processors vor begin(): NotInitialized, Eingabe unverändert.
[[nodiscard]] inline const char* checkProcessorPreBeginContract(
    IMeasurementProcessor& processor) noexcept {
    if (processor.id() == InvalidComponentId) {
        return "Processor: id() darf nicht InvalidComponentId sein";
    }
    Measurement input{};
    input.sourceId = 1;
    input.value = 1.0F;
    const Measurement inputCopy = input;
    Measurement output{};
    if (processor.process(input, output).code != StatusCode::NotInitialized) {
        return "Processor: process() vor begin() muss NotInitialized melden";
    }
    if (input.sourceId != inputCopy.sourceId || input.value != inputCopy.value) {
        return "Processor: process() darf die Eingabe nicht verändern";
    }
    return nullptr;
}

/// Vertrag eines Sinks vor begin(): NotInitialized.
[[nodiscard]] inline const char* checkSinkPreBeginContract(
    IMeasurementSink& sink) noexcept {
    if (sink.id() == InvalidComponentId) {
        return "Sink: id() darf nicht InvalidComponentId sein";
    }
    if (sink.update(0).code != StatusCode::NotInitialized) {
        return "Sink: update() vor begin() muss NotInitialized melden";
    }
    Measurement measurement{};
    measurement.sourceId = 1;
    if (sink.submit(measurement).code != StatusCode::NotInitialized) {
        return "Sink: submit() vor begin() muss NotInitialized melden";
    }
    return nullptr;
}

}  // namespace mea::testing
