#ifndef app_state_h
#define app_state_h

#include "Giessanlage.h"

class Secrets;

using Channel = Giessanlage::Channel;

// Core watering state machine. Shared by the IO subsystem (pump outputs,
// button toggles) and the MQTT subsystem (status snapshots, config apply).
extern Giessanlage anlage;

// Persistent credentials (WiFi + MQTT). Shared by the WiFi subsystem
// (SSID/pass) and the MQTT subsystem (broker/user/pass). Owned here;
// constructed by initSecrets() at boot — null until then.
extern Secrets *secrets;

// Bring up the Secrets store and load build-time credentials. Backed by
// NVS, falling back to a RAM-only store if NVS init fails so the device
// still runs on build-time credentials this boot. Call once from setup().
void initSecrets();

#endif
