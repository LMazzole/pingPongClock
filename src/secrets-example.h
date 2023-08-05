/**
 * @file secrets-example.h
 * @brief Dummy Secrets used as example for this project
 * Rename to secrets.h to use in your project
 *
 */

#pragma once

//=============WLAN CONNECTION==================================
const char* WIFI_SSID = "REPLACE_WITH_YOUR_SSID";          ///< SSID to connect to
const char* WIFI_PASSWORD = "REPLACE_WITH_YOUR_PASSWORD";  ///< Password to corresponding SSID

//=============NTP CONNECTION==================================
const char* NTP_SERVERNAME = "ch.pool.ntp.org";  //< "time.nist.gov"

///=============MQTT CONNECTION==================================
// MQTT broker credentials (set to NULL if not required)
const char* MQTT_USERNAME = "REPLACE_WITH_MQTT_USERNAME";  ///< MQTT server username
const char* MQTT_PASSWORD = "REPLACE_WITH_MQTT_PASSWORD";  ///< MQTT server password

// Change the variable to your Raspberry Pi IP address, so it connects to your MQTT broker
const char* MQTT_SERVER = "YOUR_BROKER_IP_ADDRESS";  ///< MQTT server ip adress