#pragma once

/// @file ComponentRegistry.h
/// @brief Nicht besitzende, feste Registry aus {ComponentId, erlaubte Kinds}
///        (siehe PROTOCOL-SPEC §6). Beantwortet, ob eine Komponente bekannt ist
///        und einen MessageKind senden/empfangen darf. Keine Allokation.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "MessageKind.h"

namespace mea {
namespace protocol {

/// @tparam Capacity Maximale Anzahl registrierbarer Komponenten (> 0).
template <std::size_t Capacity>
class ComponentRegistry {
    static_assert(Capacity > 0, "ComponentRegistry benötigt Kapazität > 0");

public:
    struct Entry {
        ComponentId componentId{InvalidComponentId};
        std::uint16_t allowedKinds{0};  ///< Bitmaske aus kindBit()
    };

    /// Registriert @p componentId mit erlaubten Kinds. Fehler: InvalidArgument
    /// (ID 0), DuplicateId, CapacityExceeded.
    [[nodiscard]] Status add(const ComponentId componentId,
                             const std::uint16_t allowedKinds) noexcept {
        if (componentId == InvalidComponentId) {
            return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
        }
        if (find(componentId) != nullptr) {
            return makeStatus(StatusCode::DuplicateId, componentId);
        }
        if (count_ >= Capacity) {
            return makeStatus(StatusCode::CapacityExceeded, componentId);
        }
        entries_[count_].componentId = componentId;
        entries_[count_].allowedKinds = allowedKinds;
        ++count_;
        return okStatus();
    }

    /// Eintrag zu @p componentId oder nullptr.
    [[nodiscard]] const Entry* find(const ComponentId componentId) const noexcept {
        for (std::size_t index = 0; index < count_; ++index) {
            if (entries_[index].componentId == componentId) {
                return &entries_[index];
            }
        }
        return nullptr;
    }

    /// True, wenn @p componentId registriert ist.
    [[nodiscard]] bool contains(const ComponentId componentId) const noexcept {
        return find(componentId) != nullptr;
    }

    /// True, wenn @p componentId registriert ist und @p kind erlaubt.
    [[nodiscard]] bool allows(const ComponentId componentId,
                              const MessageKind kind) const noexcept {
        const Entry* entry = find(componentId);
        if (entry == nullptr) {
            return false;
        }
        return (entry->allowedKinds & kindBit(kind)) != 0U;
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    Entry entries_[Capacity]{};
    std::size_t count_{0};
};

}  // namespace protocol
}  // namespace mea
