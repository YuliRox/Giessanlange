#ifndef ota_glue_h
#define ota_glue_h

// Start the ArduinoOTA listener if a password is configured (see
// Secrets::otaPass()). Requires initSecrets() and wifiSetup() to have run
// first. No-ops (with a serial log) if no OTA password is set, so the
// listener is disabled by default rather than accepting unauthenticated
// flashes from anyone on the LAN.
void otaSetup();

// Service pending OTA traffic; call once per loop() iteration. Safe to call
// even if otaSetup() left the listener disabled.
void otaTick();

#endif
