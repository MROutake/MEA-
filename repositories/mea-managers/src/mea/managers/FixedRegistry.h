#pragma once

/// @file FixedRegistry.h
/// @brief Generische, nicht besitzende Registry mit fester Kapazität (ADR 0001).
///        Registrierte Objekte müssen die Registry überleben.

#include <cstddef>

#include <MeaCore.h>

namespace mea {

/// @tparam Interface Interface-Typ mit `ComponentId id() const noexcept`.
/// @tparam Capacity  Maximale Anzahl registrierbarer Komponenten (> 0).
template <typename Interface, std::size_t Capacity>
class FixedRegistry {
    static_assert(Capacity > 0, "FixedRegistry benötigt Kapazität > 0");

public:
    /// Registriert eine Komponente (keine Besitzübernahme).
    /// Fehler: InvalidArgument (ID 0), DuplicateId, CapacityExceeded.
    [[nodiscard]] Status add(Interface& item) noexcept {
        const ComponentId itemId = item.id();
        if (itemId == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
        }
        if (find(itemId) != nullptr) {
            return makeStatus(StatusCode::DuplicateId, itemId);
        }
        if (count_ >= Capacity) {
            return makeStatus(StatusCode::CapacityExceeded, itemId);
        }
        items_[count_] = &item;
        ++count_;
        return okStatus();
    }

    /// Lineare Suche nach ID; nullptr, wenn nicht registriert. Deterministisch.
    [[nodiscard]] Interface* find(const ComponentId id) const noexcept {
        for (std::size_t index = 0; index < count_; ++index) {
            if (items_[index]->id() == id) {
                return items_[index];
            }
        }
        return nullptr;
    }

    /// Zugriff in Registrierungsreihenfolge; nullptr bei index >= size().
    [[nodiscard]] Interface* at(const std::size_t index) const noexcept {
        return index < count_ ? items_[index] : nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    Interface* items_[Capacity]{};
    std::size_t count_{0};
};

}  // namespace mea
