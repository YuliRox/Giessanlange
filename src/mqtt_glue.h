#ifndef mqtt_glue_h
#define mqtt_glue_h

class MqttEvents;

// Edge/event publisher. Owned by the MQTT subsystem; also used by the IO
// subsystem to publish button presses. Null until mqttSetup() runs — call
// sites must null-check. Other MQTT objects (status, config, client) are
// internal to the MQTT translation unit.
extern MqttEvents *mqttEvents;

// Bring up MQTT config (NVS-backed, applied to the state machine
// immediately), status/event publishers, and the broker client. Requires
// initSecrets() to have run first.
void mqttSetup();

// Maintain the broker connection and publish status/events for this tick.
void mqttTick(unsigned long nowMs);

#endif
