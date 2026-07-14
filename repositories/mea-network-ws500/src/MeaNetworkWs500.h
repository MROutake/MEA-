#pragma once

/// @file MeaNetworkWs500.h
/// @brief Sammel-Header von mea-network-ws500: WS500-Transportadapter für den
///        MEA-Netzwerk-Stack. Implementiert nur INetworkTransport (Byte-Ebene);
///        enthält keinerlei Protokoll-Encode/Decode-Logik (harte Regel).
///        ArduinoWs500Client ist nur unter ARDUINO verfügbar.
///
/// Abhängigkeiten: mea-core, mea-network-core.

#include "mea/network/ws500/ArduinoWs500Client.h"
#include "mea/network/ws500/IWs500Client.h"
#include "mea/network/ws500/Version.h"
#include "mea/network/ws500/Ws500Config.h"
#include "mea/network/ws500/Ws500Result.h"
#include "mea/network/ws500/Ws500Transport.h"
