#include <Arduino.h>

#include "Application.h"

namespace {
app::Application application;
}  // namespace

void setup() {
    application.begin();
}

void loop() {
    application.update(millis());
}
