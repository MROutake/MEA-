#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <MeaCore.h>
#include <IOutputDriver.h>
#include <IShiftWriter.h>
#include "74hc595Config.h"

namespace mea {

template<std::size_t RegisterCount>
class HC595OutputDriver final : public IOutputDriver {
public:


    HC595OutputDriver(IShiftWriter& writer, const HC595Config& config) noexcept
        : writer_(writer), config_(config) {}

    [[nodiscard]] ComponentId id() const noexcept override {
        return config_.id;
    }

    Status begin() noexcept override {
        initialized_ = false;

        if (config_.id == InvalidComponentId || RegisterCount == 0) {
            return makeStatus(StatusCode::InvalidConfiguration, config_.id);
        }

        Status s = writer_.begin();
        if (!s.ok()) {
            if (s.origin == InvalidComponentId) {
                s.origin = config_.id;
            }
            return s;
        }

        bytes_.fill(0U);
        dirty_ = config_.clearOnBegin;

        initialized_ = true;

        if (dirty_) {
            return commit();
        }

        return okStatus();
    }

    [[nodiscard]] std::size_t channelCount() const noexcept override {
        return RegisterCount * 8U;
    }

    Status setChannel(std::size_t channel, bool state) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, config_.id);
        }

        if (channel >= channelCount()) {
            return makeStatus(StatusCode::InvalidArgument, config_.id);
        }

        const std::size_t byteIndex = channel / 8U;
        std::size_t bitIndex = channel % 8U;
        if (config_.reverseBitOrder) {
            bitIndex = 7U - bitIndex;
        }

        const std::uint8_t mask = static_cast<std::uint8_t>(1U << bitIndex);
        const std::uint8_t before = bytes_[byteIndex];

        if (state) {
            bytes_[byteIndex] = static_cast<std::uint8_t>(before | mask);
        } else {
            bytes_[byteIndex] = static_cast<std::uint8_t>(before & static_cast<std::uint8_t>(~mask));
        }

        if (bytes_[byteIndex] != before) {
            dirty_ = true;
        }

        return okStatus();
    }

    Status commit() noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, config_.id);
        }

        if (!dirty_) {
            return okStatus();
        }

        Status s = writer_.setLatch(false);
        if (!s.ok()) {
            if (s.origin == InvalidComponentId) {
                s.origin = config_.id;
            }
            return s;
        }

        s = writer_.write(nullptr, 0);
        if (!s.ok()) {
            if (s.origin == InvalidComponentId) {
                s.origin = config_.id;
            }
            return s;
        }

        s = writer_.write(bytes_.data(), bytes_.size());
        if (!s.ok()) {
            if (s.origin == InvalidComponentId) {
                s.origin = config_.id;
            }
            return s;
        }

        s = writer_.setLatch(true);
        if (!s.ok()) {
            if (s.origin == InvalidComponentId) {
                s.origin = config_.id;
            }
            return s;
        }

        s = writer_.write(nullptr, 0);
        if (!s.ok()) {
            if (s.origin == InvalidComponentId) {
                s.origin = config_.id;
            }
            return s;
        }

        dirty_ = false;
        return okStatus();
    }

private:
    IShiftWriter& writer_;
    HC595Config config_{};
    std::array<std::uint8_t, RegisterCount> bytes_{};
    bool initialized_{false};
    bool dirty_{false};
};

} // namespace mea