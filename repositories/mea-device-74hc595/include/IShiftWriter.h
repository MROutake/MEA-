#pragma once

#include <stddef.h>
#include <stdint.h>
#include <mea/core/Status.h>

namespace mea {

class IShiftWriter {
public:
    virtual ~IShiftWriter() = default;

    virtual Status begin() noexcept = 0;
    virtual Status setLatch(bool state) noexcept = 0;
    virtual Status write(const uint8_t* data, size_t length) noexcept = 0;
};

} // namespace mea