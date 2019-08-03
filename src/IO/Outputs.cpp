/**********************************************************************
ESP32 COMMAND STATION

COPYRIGHT (c) 2017-2019 Mike Dunston

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses
**********************************************************************/

#include "ESP32CommandStation.h"

/**********************************************************************

The ESP32 Command Station supports optional OUTPUT control of any unused pins
for custom purposes. Pins can be activited or de-activated. The default is to
set ACTIVE pins HIGH and INACTIVE pins LOW. However, this default behavior can
be inverted for any pin in which case ACTIVE=LOW and INACTIVE=HIGH.

Definitions and state (ACTIVE/INACTIVE) for pins are retained on the ESP32 and
restored on power-up. The default is to set each defined pin to active or
inactive according to its restored state. However, the default behavior can be
modified so that any pin can be forced to be either active or inactive upon
power-up regardless of its previous state before power-down.

To have this sketch utilize one or more Arduino pins as custom outputs, first
define/edit/delete output definitions using the following variation of the "Z"
command:

  <Z ID PIN IFLAG>: creates a new output ID, with specified PIN and IFLAG values.
                    if output ID already exists, it is updated with specificed
                    PIN and IFLAG.
                    Note: output state will be immediately set to ACTIVE/INACTIVE
                    and pin will be set to HIGH/LOW according to IFLAG value
                    specifcied (see below).
        returns: <O> if successful and <X> if unsuccessful (e.g. out of memory)

  <Z ID>:           deletes definition of output ID
        returns: <O> if successful and <X> if unsuccessful (e.g. ID does not exist)

  <Z>:              lists all defined output pins
        returns: <Y ID PIN IFLAG STATE> for each defined output pin or <X> if no
        output pins defined

where

  ID: the numeric ID (0-32767) of the output
  PIN: the pin number to use for the output
  STATE: the state of the output (0=INACTIVE / 1=ACTIVE)
  IFLAG: defines the operational behavior of the output based on bits 0, 1, and
  2 as follows:

    IFLAG, bit 0:   0 = forward operation (ACTIVE=HIGH / INACTIVE=LOW)
                    1 = inverted operation (ACTIVE=LOW / INACTIVE=HIGH)

    IFLAG, bit 1:   0 = state of pin restored on power-up to either ACTIVE or
                        INACTIVE depending on state before power-down; state of
                        pin set to INACTIVE when first created.
                    1 = state of pin set on power-up, or when first created, to
                        either ACTIVE of INACTIVE depending on IFLAG, bit 2.

    IFLAG, bit 2:   0 = state of pin set to INACTIVE upon power-up or when
                        first created.
                    1 = state of pin set to ACTIVE upon power-up or when
                        first created.

Once all outputs have been properly defined, use the <E> command to store their
definitions. If you later make edits/additions/deletions to the output
definitions, you must invoke the <E> command if you want those new definitions
updated on the ESP32.  You can also clear everything stored on the ESP32 by
invoking the <e> command.

To change the state of outputs that have been defined use:

  <Z ID STATE>:     sets output ID to either ACTIVE or INACTIVE state
                    returns: <Y ID STATE>, or <X> if turnout ID does not exist
where
  ID: the numeric ID (0-32767) of the turnout to control
  STATE: the state of the output (0=INACTIVE / 1=ACTIVE)

When controlled as such, the ESP32 updates and stores the direction of each
output so that it is retained even without power. A list of the current states
of each output in the form <Y ID STATE> is generated by this sketch whenever the
<s> status command is invoked.  This provides an efficient way of initializing
the state of any outputs being monitored or controlled by a separate interface
or GUI program.

**********************************************************************/
#if ENABLE_OUTPUTS
LinkedList<Output *> outputs([](Output *output) {delete output; });

static constexpr const char * OUTPUTS_JSON_FILE = "outputs.json";

void OutputManager::init() {
  LOG(INFO, "[Output] Initializing outputs");
  if(configStore->exists(OUTPUTS_JSON_FILE)) {
    JsonObject root = configStore->load(OUTPUTS_JSON_FILE);
    if(root.containsKey(OUTPUTS_JSON_FILE) && root[OUTPUTS_JSON_FILE].as<int>() > 0) {
      uint16_t outputCount = root[OUTPUTS_JSON_FILE].as<int>();
      infoScreen->replaceLine(INFO_SCREEN_ROTATING_STATUS_LINE, "Found %02d Outputs", outputCount);
      for(JsonVariant output : root[JSON_OUTPUTS_NODE].as<JsonArray>()) {
        outputs.add(new Output(output.as<JsonObject>()));
      }
    }
  }
  LOG(INFO, "[Output] Loaded %d outputs", outputs.length());
}

void OutputManager::clear() {
  outputs.free();
}

uint16_t OutputManager::store() {
  JsonObject root = configStore->createRootNode();
  JsonArray array = root.createNestedArray(JSON_OUTPUTS_NODE);
  uint16_t outputStoredCount = 0;
  for (const auto& output : outputs) {
    output->toJson(array.createNestedObject());
    outputStoredCount++;
  }
  root[JSON_COUNT_NODE] = outputStoredCount;
  configStore->store(OUTPUTS_JSON_FILE, root);
  return outputStoredCount;
}

bool OutputManager::set(uint16_t id, bool active) {
  for (const auto& output : outputs) {
    if(output->getID() == id) {
      output->set(active);
      return true;
    }
  }
  return false;
}

Output *OutputManager::getOutput(uint16_t id) {
  for (const auto& output : outputs) {
    if(output->getID() == id) {
      return output;
    }
  }
  return nullptr;
}

bool OutputManager::toggle(uint16_t id) {
  for (const auto& output : outputs) {
    if(output->getID() == id) {
      output->set(!output->isActive());
      return true;
    }
  }
  return false;
}

void OutputManager::getState(JsonArray array) {
  for (const auto& output : outputs) {
    JsonObject outputJson = array.createNestedObject();
    output->toJson(outputJson, true);
  }
}

void OutputManager::showStatus() {
  for (const auto& output : outputs) {
    output->showStatus();
  }
}

bool OutputManager::createOrUpdate(const uint16_t id, const uint8_t pin, const uint8_t flags) {
  for (const auto& output : outputs) {
    if(output->getID() == id) {
      output->update(pin, flags);
      return true;
    }
  }
  if(is_restricted_pin(pin)) {
    return false;
  }
  outputs.add(new Output(id, pin, flags));
  return true;
}

bool OutputManager::remove(const uint16_t id) {
  Output *outputToRemove = nullptr;
  for (const auto& output : outputs) {
    if(output->getID() == id) {
      outputToRemove = output;
    }
  }
  if(outputToRemove != nullptr) {
    LOG(INFO, "[Output] Removing Output(%d)", outputToRemove->getID());
    outputs.remove(outputToRemove);
    return true;
  }
  return false;
}

Output::Output(uint16_t id, uint8_t pin, uint8_t flags) : _id(id), _pin(pin), _flags(flags), _active(false) {
  if(bitRead(_flags, OUTPUT_IFLAG_RESTORE_STATE)) {
    if(bitRead(_flags, OUTPUT_IFLAG_FORCE_STATE)) {
      set(true, false);
    } else {
      set(false, false);
    }
  } else {
    set(false, false);
  }
  LOG(VERBOSE, "[Output] Output(%d) on pin %d created, flags: %s", _id, _pin, getFlagsAsString().c_str());
  pinMode(_pin, OUTPUT);
}

Output::Output(JsonObject json) {
  _id = json[JSON_ID_NODE];
  _pin = json[JSON_PIN_NODE];
  _flags = json[JSON_FLAGS_NODE];
  if(bitRead(_flags, OUTPUT_IFLAG_RESTORE_STATE)) {
    if(bitRead(_flags, OUTPUT_IFLAG_FORCE_STATE)) {
      set(true, false);
    } else {
      set(false, false);
    }
  } else {
    if(json[JSON_STATE_NODE].as<bool>()) {
      set(true, false);
    } else {
      set(false, false);
    }
  }
  LOG(VERBOSE, "[Output] Output(%d) on pin %d loaded, flags: %s", _id, _pin, getFlagsAsString().c_str());
  pinMode(_pin, OUTPUT);
}

void Output::set(bool active, bool announce) {
  _active = active;
  digitalWrite(_pin, _active);
  LOG(INFO, "[Output] Output(%d) set to %s", _id, _active ? JSON_VALUE_ON : JSON_VALUE_OFF);
  if(announce) {
    wifiInterface.broadcast(StringPrintf("<Y %d %d>", _id, !_active));
  }
}

void Output::update(uint8_t pin, uint8_t flags) {
  _pin = pin;
  _flags = flags;
  if(!bitRead(_flags, OUTPUT_IFLAG_RESTORE_STATE)) {
    set(false, false);
  } else {
    if(bitRead(_flags, OUTPUT_IFLAG_FORCE_STATE)) {
      set(true, false);
    } else {
      set(false, false);
    }
  }
  LOG(VERBOSE, "[Output] Output(%d) on pin %d updated, flags: %s", _id, _pin, getFlagsAsString().c_str());
  pinMode(_pin, OUTPUT);
}

void Output::toJson(JsonObject json, bool readableStrings) {
  json[JSON_ID_NODE] = _id;
  json[JSON_PIN_NODE] = _pin;
  if(readableStrings) {
    json[JSON_FLAGS_NODE] = getFlagsAsString();
    if(isActive()) {
      json[JSON_STATE_NODE] = JSON_VALUE_ON;
    } else {
      json[JSON_STATE_NODE] = JSON_VALUE_OFF;
    }
  } else {
    json[JSON_FLAGS_NODE] = _flags;
    json[JSON_STATE_NODE] = _active;
  }

}

void Output::showStatus() {
  wifiInterface.broadcast(StringPrintf("<Y %d %d %d %d>", _id, _pin, _flags, !_active));
}

void OutputCommandAdapter::process(const vector<string> arguments) {
  if(arguments.empty()) {
    // list all outputs
    OutputManager::showStatus();
  } else {
    uint16_t outputID = std::stoi(arguments[0]);
    if (arguments.size() == 1 && OutputManager::remove(outputID)) {
      // delete output
      wifiInterface.broadcast(COMMAND_SUCCESSFUL_RESPONSE);
    } else if (arguments.size() == 2 && OutputManager::set(outputID, arguments[1][0] == 1)) {
      // set output state
    } else if (arguments.size() == 3) {
      // create output
      OutputManager::createOrUpdate(outputID, std::stoi(arguments[1]), std::stoi(arguments[2]));
      wifiInterface.broadcast(COMMAND_SUCCESSFUL_RESPONSE);
    } else {
      wifiInterface.broadcast(COMMAND_FAILED_RESPONSE);
    }
  }
}

void OutputExCommandAdapter::process(const vector<string> arguments) {
  if(arguments.empty()) {
    wifiInterface.broadcast(COMMAND_FAILED_RESPONSE);
  } else {
    uint16_t outputID = std::stoi(arguments[0]);
    auto output = OutputManager::getOutput(outputID);
    if(output) {
      output->set(!output->isActive());
    } else {
      wifiInterface.broadcast(COMMAND_FAILED_RESPONSE);
    }
  }
}
#endif // ENABLE_OUTPUTS