#pragma once

/// @file MeaNetworkCore.h
/// @brief Sammel-Header von mea-network-core: transport-neutraler
///        Netzwerk-Stack (Transport-Interface, Session, ProtocolBridge,
///        protokoll-bewusster Sink). Kennt kein konkretes Medium (WS500 etc.).
///
/// Abhängigkeiten: mea-core, mea-protocol.

#include "mea/network/IMessagePublisher.h"
#include "mea/network/INetworkTransport.h"
#include "mea/network/NetworkMeasurementSink.h"
#include "mea/network/NetworkMetrics.h"
#include "mea/network/NetworkSession.h"
#include "mea/network/ProtocolBridge.h"
#include "mea/network/ReconnectPolicy.h"
#include "mea/network/Version.h"
