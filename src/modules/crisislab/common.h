#pragma once

#include <Preferences.h>
#include <vector>
#include <string>

#include "../../mesh/generated/meshtastic/crisislab.pb.h"

// given a list of keys in the ESP32 preferences, log the value of each setting and its name
extern void logPreferences(const std::vector<std::string> &prefNames);

// for now this is the one function that will be called to handle each message,
// later it may need to be split into multiple functions.
extern void handleCrisislabMessage(const meshtastic_CrisislabMessage &message);
