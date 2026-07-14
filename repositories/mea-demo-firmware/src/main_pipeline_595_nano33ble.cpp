#include <Arduino.h>

#include <MeaCore.h>
#include <MeaManagers.h>
#include <MeaStateMachine.h>
#include <mea-device-74hc595.h>

namespace {

constexpr mea::ComponentId kSourceId = 610;
constexpr mea::ComponentId kSinkId = 620;
constexpr mea::ComponentId kPipelineId = 630;

constexpr mea::TimestampMs kSourceIntervalMs = 120;
constexpr mea::TimestampMs kPipelineCycleIntervalMs = 20;

enum class SchedulerPhase : uint8_t {
    AcquireSensors = 0,
    HandleCommands,
    RunPipeline,
    ApplyOutputs,
    Publish
};

class TickSource final : public mea::IMeasurementSource {
public:
    [[nodiscard]] mea::ComponentId id() const noexcept override { return kSourceId; }

    mea::Status begin() noexcept override {
        initialized_ = true;
        first_ = true;
        pending_ = false;
        sequence_ = 0;
        lastTickMs_ = 0;
        return mea::okStatus();
    }

    mea::Status update(mea::TimestampMs nowMs) noexcept override {
        if (!initialized_) {
            return mea::makeStatus(mea::StatusCode::NotInitialized, kSourceId);
        }
        if (pending_) {
            return mea::okStatus();
        }

        if (first_ || mea::intervalElapsed(nowMs, lastTickMs_, kSourceIntervalMs)) {
            first_ = false;
            lastTickMs_ = nowMs;

            pendingMeasurement_ = {};
            pendingMeasurement_.sourceId = kSourceId;
            pendingMeasurement_.kind = mea::MeasurementKind::Unknown;
            pendingMeasurement_.unit = mea::Unit::None;
            pendingMeasurement_.value = static_cast<float>(sequence_);
            pendingMeasurement_.sampledAtMs = nowMs;
            pendingMeasurement_.sequence = sequence_;
            pendingMeasurement_.quality = mea::QualityFlag::None;

            ++sequence_;
            pending_ = true;
        }

        return mea::okStatus();
    }

    [[nodiscard]] size_t available() const noexcept override { return pending_ ? 1U : 0U; }

    mea::Status read(mea::Measurement& output) noexcept override {
        if (!initialized_) {
            return mea::makeStatus(mea::StatusCode::NotInitialized, kSourceId);
        }
        if (!pending_) {
            return mea::makeStatus(mea::StatusCode::NoData, kSourceId);
        }
        output = pendingMeasurement_;
        pending_ = false;
        return mea::okStatus();
    }

private:
    bool initialized_{false};
    bool first_{true};
    bool pending_{false};
    mea::TimestampMs lastTickMs_{0};
    mea::SequenceNumber sequence_{0};
    mea::Measurement pendingMeasurement_{};
};

class RunlightSink final : public mea::IMeasurementSink {
public:
    RunlightSink(mea::HC595OutputDriver<2>& driver) noexcept : driver_(driver) {}

    [[nodiscard]] mea::ComponentId id() const noexcept override { return kSinkId; }

    mea::Status begin() noexcept override {
        const mea::Status status = driver_.begin();
        initialized_ = status.ok();
        currentPos_ = 0;
        hasActivePos_ = false;
        hasPendingPos_ = false;
        return status;
    }

    mea::Status update(mea::TimestampMs) noexcept override {
        if (!initialized_) {
            return mea::makeStatus(mea::StatusCode::NotInitialized, kSinkId);
        }

        if (!hasPendingPos_) {
            return mea::okStatus();
        }

        if (hasActivePos_) {
            mea::Status status = driver_.setChannel(currentPos_, false);
            if (!status.ok()) {
                return status;
            }
        }

        mea::Status status = driver_.setChannel(pendingPos_, true);
        if (!status.ok()) {
            return status;
        }

        status = driver_.commit();
        if (!status.ok()) {
            return status;
        }

        currentPos_ = pendingPos_;
        hasActivePos_ = true;
        hasPendingPos_ = false;
        return mea::okStatus();
    }

    [[nodiscard]] size_t capacityAvailable() const noexcept override {
        return (initialized_ && !hasPendingPos_) ? 1U : 0U;
    }

    mea::Status submit(const mea::Measurement& measurement) noexcept override {
        if (!initialized_) {
            return mea::makeStatus(mea::StatusCode::NotInitialized, kSinkId);
        }

        if (hasPendingPos_) {
            return mea::makeStatus(mea::StatusCode::WouldBlock, kSinkId);
        }

        const size_t channels = driver_.channelCount();
        if (channels == 0) {
            return mea::makeStatus(mea::StatusCode::InvalidConfiguration, kSinkId);
        }

        pendingPos_ = static_cast<size_t>(measurement.sequence % channels);
        hasPendingPos_ = true;
        return mea::okStatus();
    }

private:
    mea::HC595OutputDriver<2>& driver_;
    bool initialized_{false};
    bool hasActivePos_{false};
    bool hasPendingPos_{false};
    size_t currentPos_{0};
    size_t pendingPos_{0};
};

constexpr mea::ComponentId kSinkIds[] = {kSinkId};

mea::PipelineConfig makePipelineConfig() noexcept {
    return mea::makePipelineConfig(
        kPipelineId, kSourceId,
        mea::ArrayView<const mea::ComponentId>(nullptr, 0),
        mea::ArrayView<const mea::ComponentId>(kSinkIds, 1),
        kPipelineCycleIntervalMs, 1000, 1000, {50, 2}, true);
}


mea::ArduinoShiftWriterConfig writerConfig{
    A3,   // dataPin
    A0,   // shiftClockPin
    A1,   // storageClockPin
    A2,   // enablePin (OE, active LOW)
    A4,   // masterResetPin (MR, active LOW)
    0    // bitOrder: 0 = MSBFIRST
};

mea::Arduino74hc595 writer(writerConfig);
mea::HC595Config driverConfig{kSinkId, true, false};
mea::HC595OutputDriver<2> outputDriver(writer, driverConfig);

TickSource source;
RunlightSink sink(outputDriver);

mea::SensorManager<1> sources;
mea::ProcessorManager<1> processors;
mea::SinkManager<1> sinks;
mea::MeasurementPipelineMachine pipeline(sources, processors, sinks, makePipelineConfig());

mea::DefaultCycleOrchestrator::Config orchestratorConfig{
    {
        1000,  // acquireSensorsMs
        1000,  // handleCommandsMs
        1000,  // runPipelineMs
        1000,  // applyOutputsMs
        1000   // publishMs
    },
    true,  // skipPipelineIfNoData
    true   // continueAfterNonTransientError
};

mea::DefaultCycleOrchestrator orchestrator(sources, sinks, pipeline, source,
                                           orchestratorConfig, kPipelineId);

bool orchestratorInitialized = false;
mea::StatusCode lastReportedCode = mea::StatusCode::Ok;

void reportStatus(const char* stage, const mea::Status& status) {
    Serial.print("[pipeline-595] ");
    Serial.print(stage);
    Serial.print(": ");
    Serial.print(mea::statusCodeName(status.code));
    Serial.print(" origin=");
    Serial.print(status.origin);
    Serial.print(" detail=");
    Serial.println(status.detail);
}

}  // namespace

void setup() {
    Serial.begin(115200);

    mea::Status status = sources.registerComponent(source);
    if (!status.ok()) {
        reportStatus("register source", status);
        return;
    }

    status = sinks.registerComponent(sink);
    if (!status.ok()) {
        reportStatus("register sink", status);
        return;
    }

    status = sources.beginAll();
    if (!status.ok()) {
        reportStatus("sources beginAll", status);
        return;
    }

    status = processors.beginAll();
    if (!status.ok()) {
        reportStatus("processors beginAll", status);
        return;
    }

    status = sinks.beginAll();
    if (!status.ok()) {
        reportStatus("sinks beginAll", status);
        return;
    }

    status = pipeline.begin(millis());
    if (!status.ok()) {
        reportStatus("pipeline begin", status);
        return;
    }

    const mea::Status orchestratorStatus = orchestrator.begin(millis());
    if (!orchestratorStatus.ok()) {
        reportStatus("orchestrator begin", orchestratorStatus);
        return;
    }

    orchestratorInitialized = true;
}

void loop() {
    if (!orchestratorInitialized) {
        return;
    }

    const mea::Status status = orchestrator.update(millis());
    if (!status.ok() && !status.transient()) {
        if (status.code != lastReportedCode) {
            lastReportedCode = status.code;
            reportStatus(mea::DefaultCycleOrchestrator::phaseName(orchestrator.phase()),
                         status);
        }
    } else if (status.ok() || status.transient()) {
        lastReportedCode = mea::StatusCode::Ok;
    }
}
