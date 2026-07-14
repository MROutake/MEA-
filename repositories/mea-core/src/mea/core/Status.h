#pragma once

/// @file Status.h
/// @brief Status- und Fehlermodell (ADR 0002): Statuscode + Herkunft + Detail.

#include <stdint.h>

#include "Types.h"

namespace mea {

/// Ergebniscode einer Operation. Der Operationsstatus ist unabhängig von der
/// Qualität transportierter Messdaten (siehe Measurement::quality, ADR 0003).
enum class StatusCode : uint8_t {
    Ok = 0,
    Busy,
    NoData,
    WouldBlock,
    NotInitialized,
    AlreadyInitialized,
    InvalidArgument,
    InvalidConfiguration,
    CapacityExceeded,
    DuplicateId,
    NotFound,
    Timeout,
    IoError,
    ProtocolError,
    ChecksumError,
    ProcessingError,
    Unsupported,
    InternalError
};

/// Operationsergebnis mit Herkunft. Trivial kopierbar, keine dynamischen Texte.
struct Status {
    StatusCode code{StatusCode::Ok};
    /// Komponente, die den Fehler meldet (InvalidComponentId, wenn unbekannt).
    ComponentId origin{InvalidComponentId};
    /// Geräte- oder protokollspezifische Zusatzinformation.
    uint16_t detail{0};

    [[nodiscard]] constexpr bool ok() const noexcept { return code == StatusCode::Ok; }

    /// Transiente Zustände: Wiederholen ohne Eingriff ist sinnvoll (ADR 0002).
    [[nodiscard]] constexpr bool transient() const noexcept {
        return code == StatusCode::Busy || code == StatusCode::NoData ||
               code == StatusCode::WouldBlock;
    }
};

/// Erzeugt einen Status mit Herkunft.
[[nodiscard]] constexpr Status makeStatus(const StatusCode code, const ComponentId origin,
                                          const uint16_t detail = 0) noexcept {
    return Status{code, origin, detail};
}

/// Erfolgs-Status ohne Herkunft.
[[nodiscard]] constexpr Status okStatus() noexcept {
    return Status{};
}

/// Menschlich lesbarer Name eines Statuscodes (statisches Stringliteral).
[[nodiscard]] constexpr const char* statusCodeName(const StatusCode code) noexcept {
    switch (code) {
        case StatusCode::Ok:
            return "Ok";
        case StatusCode::Busy:
            return "Busy";
        case StatusCode::NoData:
            return "NoData";
        case StatusCode::WouldBlock:
            return "WouldBlock";
        case StatusCode::NotInitialized:
            return "NotInitialized";
        case StatusCode::AlreadyInitialized:
            return "AlreadyInitialized";
        case StatusCode::InvalidArgument:
            return "InvalidArgument";
        case StatusCode::InvalidConfiguration:
            return "InvalidConfiguration";
        case StatusCode::CapacityExceeded:
            return "CapacityExceeded";
        case StatusCode::DuplicateId:
            return "DuplicateId";
        case StatusCode::NotFound:
            return "NotFound";
        case StatusCode::Timeout:
            return "Timeout";
        case StatusCode::IoError:
            return "IoError";
        case StatusCode::ProtocolError:
            return "ProtocolError";
        case StatusCode::ChecksumError:
            return "ChecksumError";
        case StatusCode::ProcessingError:
            return "ProcessingError";
        case StatusCode::Unsupported:
            return "Unsupported";
        case StatusCode::InternalError:
            return "InternalError";
    }
    return "Unknown";
}

static_assert(sizeof(Status) <= 6, "Status muss klein und trivial kopierbar bleiben");

}  // namespace mea
