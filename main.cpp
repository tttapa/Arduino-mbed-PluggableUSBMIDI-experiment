#include <Arduino.h>
#include "PluggableUSBMIDI.hpp"

PluggableUSBMIDI midi;

void setup() {
    Serial1.begin(115'200);
    Serial1.print("\r\n\r\n-----\r\n\r\n");
}

void loop() {
    midi.read_and_verify();
}