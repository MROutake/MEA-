#pragma once

/// @file Interfaces.h
/// @brief Kerninterfaces der Plattform. Komponenten bleiben Eigentum des
///        Composition Roots (ADR 0001); update() blockiert nie (ADR 0004).

#include <cstddef>

#include "Measurement.h"
#include "Status.h"
#include "Types.h"

namespace mea {

/// Quelle von Messwerten (z. B. Sensor).
/// - update() blockiert nicht und leistet pro Aufruf nur begrenzte Arbeit.
/// - available() nennt die Anzahl auslesbarer Messwerte.
/// - read() entnimmt genau einen Wert (NoData, wenn keiner vorliegt).
/// - begin() reinitialisiert bei erneutem Aufruf (dokumentiert je Komponente, ADR 0004).
class IMeasurementSource {
public:
    virtual ~IMeasurementSource() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;
    [[nodiscard]] virtual std::size_t available() const noexcept = 0;
    virtual Status read(Measurement& output) noexcept = 0;
};

/// Zustandsarme Verarbeitung eines Messwerts (Eingabe wird nie verändert).
class IMeasurementProcessor {
public:
    virtual ~IMeasurementProcessor() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;

    /// True, wenn der Prozessor Eingaben dieser Art/Einheit verarbeitet.
    [[nodiscard]] virtual bool accepts(MeasurementKind kind,
                                       Unit unit) const noexcept = 0;

    virtual Status process(const Measurement& input, Measurement& output) noexcept = 0;
};

/// Abnehmer von Messwerten (z. B. Kommunikationsausgang).
/// - submit() kopiert den Wert in einen begrenzten Puffer oder verarbeitet ihn sofort;
///   WouldBlock signalisiert Backpressure (Wert wurde nicht übernommen).
/// - capacityAvailable() darf 0 sein.
class IMeasurementSink {
public:
    virtual ~IMeasurementSink() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;
    [[nodiscard]] virtual std::size_t capacityAvailable() const noexcept = 0;
    virtual Status submit(const Measurement& measurement) noexcept = 0;
};

/// Nicht besitzende Nachschlage-Sicht auf registrierte Komponenten.
/// Wird von den Managern (mea-managers) implementiert; die State Machine kennt
/// Komponenten ausschließlich über dieses Interface und ihre IDs.
template <typename Interface>
class IComponentLocator {
public:
    virtual ~IComponentLocator() = default;

    /// nullptr, wenn keine Komponente mit @p id registriert ist.
    [[nodiscard]] virtual Interface* find(ComponentId id) const noexcept = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
};

using ISourceLocator = IComponentLocator<IMeasurementSource>;
using IProcessorLocator = IComponentLocator<IMeasurementProcessor>;
using ISinkLocator = IComponentLocator<IMeasurementSink>;

}  // namespace mea
