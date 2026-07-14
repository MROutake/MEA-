#pragma once

/// @file MeaProtocol.h
/// @brief Sammel-Header von mea-protocol: transport-unabhängiges, binäres
///        Rahmenformat für MEA-Netzwerkdaten (siehe docs/PROTOCOL-SPEC-v0.1.md).
///
/// Abhängigkeit ausschließlich auf mea-core. Kein Transport-Code (WS500,
/// Serial, TCP) in dieser Library (harte Architekturregel).

#include "mea/protocol/BinaryMessageCodec.h"
#include "mea/protocol/ByteOrder.h"
#include "mea/protocol/ComponentRegistry.h"
#include "mea/protocol/Crc16.h"
#include "mea/protocol/IMessageCodec.h"
#include "mea/protocol/MessageEnvelope.h"
#include "mea/protocol/MessageHeader.h"
#include "mea/protocol/MessageKind.h"
#include "mea/protocol/MessageValidator.h"
#include "mea/protocol/Payloads.h"
#include "mea/protocol/Version.h"
