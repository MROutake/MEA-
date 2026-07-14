#pragma once

#include <stdint.h>

namespace mea{

    struct ArduinoShiftWriterConfig
    {
        uint8_t dataPin{0};
        uint8_t shiftClockPin{0};
        uint8_t storageClockPin{0};
        uint8_t enablePin{0};
        uint8_t masterResetPin{0};
        uint8_t bitOrder{0}; // 0 = MSBFIRST, 1 = LSBFIRST
        
    };
    

}// namespace mea