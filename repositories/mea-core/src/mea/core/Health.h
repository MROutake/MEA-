#pragma once

/// @file Health.h
/// @brief Diagnosemodell. Wird von Managern gepflegt (ADR 0004); Komponenten
///        benötigen dafür keine zusätzlichen virtuellen Interfaces.

#include <cstdint>

#include "Status.h"
#include "Types.h"

namespace mea {

/// Laufzeitdiagnose einer registrierten Komponente. Trivial kopierbar.
struct ComponentHealth {
    ComponentId componentId{InvalidComponentId};
    Status lastStatus{};
    TimestampMs lastSuccessMs{0};
    std::uint32_t successCount{0};
    std::uint32_t errorCount{0};
};

/// Verbucht ein Operationsergebnis. Transiente Status zählen nicht als Fehler.
inline void recordResult(ComponentHealth& health, const Status status,
                         const TimestampMs nowMs) noexcept {
    health.lastStatus = status;
    if (status.ok()) {
        health.lastSuccessMs = nowMs;
        ++health.successCount;
    } else if (!status.transient()) {
        ++health.errorCount;
    }
}

}  // namespace mea
