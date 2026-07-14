#pragma once

#include <stdint.h>
#include <mea/core/Types.h>

namespace mea {
struct HC595Config {

    ComponentId id{InvalidComponentId};
    bool clearOnBegin{true};
    bool reverseBitOrder{false};

};
} // namespace mea