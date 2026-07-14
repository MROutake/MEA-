#include "Application.h"

#include <Arduino.h>

#include "AppIds.h"
#include "BoardConfig.h"

namespace app {

namespace {

/// Pipeline-Verdrahtung über IDs. Statische Lebensdauer: die Arrays müssen
/// länger leben als die State Machine (ADR 0001).
constexpr mea::ComponentId kProcessorIds[] = {ids::RawToVoltage, ids::VoltageClamp};
constexpr mea::ComponentId kSinkIds[] = {ids::SerialOutput};

mea::PipelineConfig pipelineConfig() noexcept {
    mea::PipelineConfig pipelineCfg{};
    pipelineCfg.pipelineId = ids::MeasurementPipeline;
    pipelineCfg.sourceId = ids::AnalogInput1;
    pipelineCfg.processorIds = mea::ArrayView<const mea::ComponentId>(kProcessorIds, 2);
    pipelineCfg.sinkIds = mea::ArrayView<const mea::ComponentId>(kSinkIds, 1);
    pipelineCfg.cycleIntervalMs = config::kPipelineCycleIntervalMs;
    pipelineCfg.acquisitionTimeoutMs = config::kAcquisitionTimeoutMs;
    pipelineCfg.publishTimeoutMs = config::kPublishTimeoutMs;
    pipelineCfg.retry = config::kRetryPolicy;
    pipelineCfg.startImmediately = config::kStartImmediately;
    return pipelineCfg;
}

}  // namespace

Application::Application() noexcept
    : analogReader_(board::kAdcMaximumRaw),
      analogSensor_(analogReader_,
                    {
                        ids::AnalogInput1,
                        board::kAnalogInputPin,
                        config::kSensorSampleIntervalMs,
                        config::kSamplesPerMeasurement,
                        config::kMaxSamplesPerUpdate,
                        mea::MeasurementKind::RawAnalog,
                        mea::Unit::RawCount,
                    }),
      rawToVoltage_({
          ids::RawToVoltage,
          config::kAdcToVoltGain,
          config::kVoltageOffset,
          mea::MeasurementKind::RawAnalog,
          mea::Unit::RawCount,
          mea::MeasurementKind::Voltage,
          mea::Unit::Volt,
      }),
      voltageClamp_({
          ids::VoltageClamp,
          config::kVoltageMin,
          config::kVoltageMax,
          mea::MeasurementKind::Voltage,
          mea::Unit::Volt,
      }),
      serialTransport_(Serial),
      csvEncoder_({';', config::kCsvDecimalPlaces}),
      serialSink_(serialTransport_, csvEncoder_, ids::SerialOutput),
      pipeline_(sources_, processors_, sinks_, pipelineConfig()) {}

void Application::reportStatus(const char* stage, const mea::Status& status) {
    Serial.print("[mea] ");
    Serial.print(stage);
    Serial.print(": ");
    Serial.print(mea::statusCodeName(status.code));
    Serial.print(" origin=");
    Serial.print(status.origin);
    Serial.print(" detail=");
    Serial.println(status.detail);
}

void Application::begin() {
    Serial.begin(board::kSerialBaudRate);

    healthy_ = false;

    // Registrierung (vor beginAll(), ADR 0004).
    mea::Status status = sources_.registerComponent(analogSensor_);
    if (!status.ok()) {
        reportStatus("register source", status);
        return;
    }
    status = processors_.registerComponent(rawToVoltage_);
    if (status.ok()) {
        status = processors_.registerComponent(voltageClamp_);
    }
    if (!status.ok()) {
        reportStatus("register processor", status);
        return;
    }
    status = sinks_.registerComponent(serialSink_);
    if (!status.ok()) {
        reportStatus("register sink", status);
        return;
    }

    // Initialisierung: Transport zuerst, dann Manager genau einmal.
    status = serialTransport_.begin();
    if (!status.ok()) {
        reportStatus("transport begin", status);
        return;
    }
    status = sources_.beginAll();
    if (!status.ok()) {
        reportStatus("sources beginAll", status);
        return;
    }
    status = processors_.beginAll();
    if (!status.ok()) {
        reportStatus("processors beginAll", status);
        return;
    }
    status = sinks_.beginAll();
    if (!status.ok()) {
        reportStatus("sinks beginAll", status);
        return;
    }

    status = pipeline_.begin(millis());
    if (!status.ok()) {
        reportStatus("pipeline begin", status);
        return;
    }

    healthy_ = true;
}

void Application::update(const mea::TimestampMs nowMs) {
    if (!healthy_) {
        return;  // begin() hat den Fehler bereits gemeldet
    }

    // Reihenfolge: Quellen erfassen, Transport/Sinks schreiben nach,
    // Pipeline koordiniert (jeweils nicht blockierend).
    const mea::Status sourceStatus = sources_.updateAll(nowMs);
    (void)serialTransport_.update(nowMs);
    const mea::Status sinkStatus = sinks_.updateAll(nowMs);
    const mea::Status pipelineStatus = pipeline_.update(nowMs);

    // Schwere Fehler genau einmal je Statuswechsel melden (kein Log-Sturm).
    const mea::Status severe = !sourceStatus.ok() ? sourceStatus
                               : !sinkStatus.ok() ? sinkStatus
                                                  : pipelineStatus;
    if (!severe.ok() && !severe.transient()) {
        if (severe.code != lastReportedCode_) {
            lastReportedCode_ = severe.code;
            reportStatus("update", severe);
        }
    } else if (severe.ok()) {
        lastReportedCode_ = mea::StatusCode::Ok;
    }
}

}  // namespace app
