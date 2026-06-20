#ifndef wifi_glue_h
#define wifi_glue_h

class WifiManager;

// WiFi connection manager. Owned by the WiFi subsystem; also read by the
// MQTT subsystem to gate connection attempts on link state. Null until
// wifiSetup() runs.
extern WifiManager *wifi;

// Configure the radio for station use and construct the WifiManager from
// the stored credentials. Requires initSecrets() to have run first.
void wifiSetup();

// Drive the connection state machine; logs transitions on serial.
void wifiTick(unsigned long elapsedMs);

#endif
