/**********************************************************************
ESP32 COMMAND STATION

COPYRIGHT (c) 2018 Dan Worth
COPYRIGHT (c) 2018-2020 Mike Dunston

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

The ESP32 Command Station supports remote sensor inputs that are connected via a
WiFi connection. Remote Sensors are dynamically created by a remote sensor
reporting its state.

Note: Remote Sensors should not maintain a persistent connection. Instead they
should connect when a change occurs that should be reported. It is not necessary
for Remote Sensors to report when they are INACTIVE. If a Remote Sensor does not
report within REMOTE_SENSORS_DECAY milliseconds the command station will
automatically transition the Remote Sensor to INACTIVE state if it was
previously ACTIVE.

The following varations of the "RS" command :

  <RS ID STATE>:      Informs the command station of the status of a remote sensor.
  <RS ID>:            Deletes remote sensor ID.
  <RS>:               Lists all defined remote sensors.
                      returns: <RS ID STATE> for each defined remote sensor or
                      <X> if no remote sensors have been defined/found.
where

  ID:     the numeric ID (0-32667) of the remote sensor.
  STATE:  State of the sensors, zero is INACTIVE, non-zero is ACTIVE.
          Usage is remote sensor dependent.
**********************************************************************/

#if CONFIG_ENABLE_SENSORS
// TODO: merge this into the base SensorManager code.

vector<unique_ptr<RemoteSensor>> remoteSensors;

void RemoteSensorManager::init()
{
}

void RemoteSensorManager::createOrUpdate(const uint16_t id, const uint16_t value) {
  // check for duplicate ID
  for (const auto& sensor : remoteSensors)
  {
    if(sensor->getRawID() == id)
    {
      sensor->setSensorValue(value);
      return;
    }
  }
  remoteSensors.push_back(std::make_unique<RemoteSensor>(id, value));
}

bool RemoteSensorManager::remove(const uint16_t id)
{
  auto ent = std::find_if(remoteSensors.begin(), remoteSensors.end(),
  [id](unique_ptr<RemoteSensor> & sensor) -> bool
  {
    return sensor->getID() == id;
  });
  if (ent != remoteSensors.end())
  {
    remoteSensors.erase(ent);
    return true;
  }
  return false;
}

string RemoteSensorManager::getStateAsJson()
{
  string output = "[";
  for (const auto& sensor : remoteSensors)
  {
    output += sensor->toJson();
    output += ",";
  }
  output += "]";
  return output;
}

string RemoteSensorManager::get_state_for_dccpp()
{
  if (remoteSensors.empty())
  {
    return COMMAND_FAILED_RESPONSE;
  }
  string status;
  for (const auto& sensor : remoteSensors)
  {
    status += sensor->get_state_for_dccpp();
  }
  return status;
}

RemoteSensor::RemoteSensor(uint16_t id, uint16_t value) :
  Sensor(id + CONFIG_REMOTE_SENSORS_FIRST_SENSOR, NON_STORED_SENSOR_PIN, false, false), _rawID(id)
{
  setSensorValue(value);
  LOG(VERBOSE, "[RemoteSensors] RemoteSensor(%d) created with Sensor(%d), active: %s, value: %d",
    getRawID(), getID(), isActive() ? JSON_VALUE_TRUE : JSON_VALUE_FALSE, value);
}

void RemoteSensor::check()
{
  if(isActive() && (esp_timer_get_time() / 1000ULL) > _lastUpdate + CONFIG_REMOTE_SENSORS_DECAY)
  {
    LOG(INFO, "[RemoteSensors] RemoteSensor(%d) expired, deactivating", getRawID());
    setSensorValue(0);
  }
}

string RemoteSensor::get_state_for_dccpp()
{
  return StringPrintf("<RS %d %d>", getRawID(), _value);
}

string RemoteSensor::toJson(bool includeState)
{
  nlohmann::json object =
  {
    { JSON_ID_NODE, getRawID() },
    { JSON_VALUE_NODE, getSensorValue() },
    { JSON_STATE_NODE, isActive() },
    { JSON_LAST_UPDATE_NODE, getLastUpdate() },
    { JSON_PIN_NODE, getPin() },
    { JSON_PULLUP_NODE, isPullUp() },
  };
  return object.dump();
}

#endif // CONFIG_ENABLE_SENSORS