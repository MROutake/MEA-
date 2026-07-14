#pragma once

/// @file MeaCommunication.h
/// @brief Sammel-Header von mea-communication. ArduinoStreamTransport ist nur
///        unter ARDUINO verfügbar; Encoder, Sink und Decoder sind nativ testbar.
///
/// Migration: Der frühere SerialCsvSink wurde durch die Kombination
/// CsvMeasurementEncoder + ArduinoStreamTransport + BufferedMeasurementSink
/// ersetzt (siehe CHANGELOG.md und ADR 0006).

#include "mea/communication/ArduinoStreamTransport.h"
#include "mea/communication/BufferedMeasurementSink.h"
#include "mea/communication/CsvMeasurementEncoder.h"
#include "mea/communication/IByteTransport.h"
#include "mea/communication/IMeasurementEncoder.h"
#include "mea/communication/LineCommandDecoder.h"
#include "mea/communication/Version.h"
