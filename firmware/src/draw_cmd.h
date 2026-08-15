#pragma once
#include <Arduino.h>

// Execute a JSON drawing command payload on the TFT.
// Returns false if the JSON is malformed.
bool drawExecute(const String &json);
