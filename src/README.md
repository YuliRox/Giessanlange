# Giessanlage Library

Manages the internal pump state. Handles pump timers and provides minimal configuration for timer duration.

```cpp

Giessanlage anlage;

// in update loop check to see if pump should be running
void loop() {
    // update internal state
    anlage.tick(time_since_last_update);

    bool isPumping = anlage.isPumping();
    if(isPumping) {
        // run electricity to pummp
    } else {
        // retrieve exposed state
        Giessanlage::State state = anlage.getState(); // should be State::Idle if not pumping
    }
}

// try to start a manual pumping process (unscheduled)
if(anlage.triggerPump()) {
    // check if pump should be running
    bool isPumping = anlage.isPumping(); //->true

    // remaining time to run the pump (ms)
    unsigned long remainingPumpTime = anlage.getRemainingPumpTime();
    // time until next scheduled pump interval starts (ms)
    unsigned long remainingWateringInterval = anlage.getRemainingWateringInterval();
}

if(anlage.stopPump()) {
    bool isPumping = anlage.isPumping(); //->false
}

```
