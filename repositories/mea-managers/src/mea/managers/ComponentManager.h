#pragma once

/// @file ComponentManager.h
/// @brief Gemeinsame Manager-Basis: Registrierung, Lebenszyklus (ADR 0004) und
///        Diagnose (ComponentHealth) für registrierte Komponenten. Manager
///        besitzen die Komponenten nicht (ADR 0001) und enthalten keine Fachlogik.

#include <cstddef>

#include <MeaCore.h>

#include "FixedRegistry.h"

namespace mea {

/// Basis für alle fachlichen Manager.
///
/// Lebenszyklus (ADR 0004):
/// - Registrierung nur vor erfolgreichem beginAll(); danach AlreadyInitialized.
/// - beginAll() initialisiert alle Komponenten genau einmal. Ein zweiter Aufruf
///   nach Erfolg meldet AlreadyInitialized (gewählte Variante). Ein fehlgeschlagenes
///   beginAll() darf wiederholt werden.
/// - Fehler tragen immer die ID der verursachenden Komponente (origin).
template <typename Interface, std::size_t Capacity>
class ComponentManager : public IComponentLocator<Interface> {
public:
    /// Registriert eine Komponente (keine Besitzübernahme, ID 0 und Duplikate abgelehnt).
    [[nodiscard]] Status registerComponent(Interface& component) noexcept {
        if (begun_) {
            return makeStatus(StatusCode::AlreadyInitialized, component.id());
        }
        const Status status = registry_.add(component);
        if (status.ok()) {
            healths_[registry_.size() - 1U] =
                ComponentHealth{component.id(), okStatus(), 0, 0, 0};
        }
        return status;
    }

    /// Initialisiert alle registrierten Komponenten. beginAll() versucht immer
    /// alle Komponenten und meldet den ersten Fehler (mit Verursacher-ID).
    [[nodiscard]] Status beginAll() noexcept {
        if (begun_) {
            return Status{StatusCode::AlreadyInitialized, InvalidComponentId, 0};
        }
        Status firstError = okStatus();
        for (std::size_t index = 0; index < registry_.size(); ++index) {
            Interface* const component = registry_.at(index);
            Status status = component->begin();
            if (!status.ok() && status.origin == InvalidComponentId) {
                status.origin = component->id();
            }
            recordResult(healths_[index], status, 0);
            if (!status.ok() && firstError.ok()) {
                firstError = status;
            }
        }
        begun_ = firstError.ok();
        return firstError;
    }

    [[nodiscard]] Interface* find(const ComponentId id) const noexcept override {
        return registry_.find(id);
    }

    [[nodiscard]] std::size_t size() const noexcept override { return registry_.size(); }

    /// Zugriff in Registrierungsreihenfolge; nullptr bei index >= size().
    [[nodiscard]] Interface* at(const std::size_t index) const noexcept {
        return registry_.at(index);
    }

    /// Diagnose einer Komponente. Unbekannte ID: componentId == InvalidComponentId
    /// und lastStatus.code == NotFound.
    [[nodiscard]] ComponentHealth health(const ComponentId id) const noexcept {
        for (std::size_t index = 0; index < registry_.size(); ++index) {
            if (registry_.at(index)->id() == id) {
                return healths_[index];
            }
        }
        ComponentHealth missing{};
        missing.lastStatus = makeStatus(StatusCode::NotFound, id);
        return missing;
    }

    [[nodiscard]] bool initialized() const noexcept { return begun_; }

protected:
    ComponentManager() = default;

    [[nodiscard]] FixedRegistry<Interface, Capacity>& registry() noexcept {
        return registry_;
    }

    /// Verbucht ein Laufzeitergebnis für die Komponente an Position @p index.
    void record(const std::size_t index, const Status status,
                const TimestampMs nowMs) noexcept {
        if (index < registry_.size()) {
            recordResult(healths_[index], status, nowMs);
        }
    }

private:
    FixedRegistry<Interface, Capacity> registry_{};
    ComponentHealth healths_[Capacity]{};
    bool begun_{false};
};

/// Manager-Basis für Komponenten mit zyklischem update(nowMs).
template <typename Interface, std::size_t Capacity>
class UpdatableComponentManager : public ComponentManager<Interface, Capacity> {
public:
    /// Aktualisiert alle Komponenten. Ein Fehler einer Komponente stoppt die
    /// anderen nicht; gemeldet wird der erste nicht transiente Fehler
    /// (inklusive Verursacher-ID), sonst Ok.
    [[nodiscard]] Status updateAll(const TimestampMs nowMs) noexcept {
        if (!this->initialized()) {
            return Status{StatusCode::NotInitialized, InvalidComponentId, 0};
        }
        Status firstError = okStatus();
        for (std::size_t index = 0; index < this->size(); ++index) {
            Interface* const component = this->at(index);
            Status status = component->update(nowMs);
            if (!status.ok() && status.origin == InvalidComponentId) {
                status.origin = component->id();
            }
            this->record(index, status, nowMs);
            if (!status.ok() && !status.transient() && firstError.ok()) {
                firstError = status;
            }
        }
        return firstError;
    }
};

}  // namespace mea
