#ifndef io_glue_h
#define io_glue_h

// Local IO: pump MOSFET outputs and the three debounced pushbuttons.
// Configure pin modes and drive the pumps to a safe-off state. Call once
// from setup(), before anything that might block, so the pumps are
// guaranteed off early in boot.
void ioSetup();

// Per-loop: mirror the state machine onto the pump outputs and poll the
// buttons (toggling pumps / cancelling and emitting MQTT button events).
// Also prints the periodic "time until next watering" line.
void ioTick(unsigned long nowMs, unsigned long elapsedMs);

#endif
