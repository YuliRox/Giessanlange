# Hardware Abstraction & Testability

## Guiding principle

The existing `Giessanlage` state machine is already platform-independent: it takes a `delta_ms` argument and never calls `millis()`. The same separation should apply to any new Arduino-facing code. Hardware-touching logic lives in thin wrapper classes; all testable logic is kept free of Arduino headers.

## The problem with direct Arduino calls

```cpp
// Not testable on the native env — digitalRead() is undefined there
bool pollPressed(int pin) {
    return digitalRead(pin) == LOW;
}
```

When `env:native` runs, there is no `digitalRead`. Any class that calls it directly cannot be compiled or tested on the host.

## Pattern: inject the pin reader

The lightest-weight fix is **dependency injection via `std::function`**. The class holds a callable instead of a pin number, and callers provide the real `digitalRead` on hardware or a lambda in tests.

```cpp
// DebouncedButton.h
#pragma once
#include <functional>

class DebouncedButton {
public:
    using PinReader = std::function<int()>;

    // debounceMs: how long the pin must be stable before the edge is reported
    DebouncedButton(PinReader reader, unsigned long debounceMs = 100);

    // Call every loop tick. Returns true exactly once per closing edge.
    bool poll(unsigned long nowMs);

    bool isPressed() const;

private:
    PinReader _reader;
    unsigned long _debounceMs;
    int _state;        // last confirmed stable state
    int _lastReading;  // raw reading from previous poll
    unsigned long _lastChangeMs;
};
```

**On hardware** (`main.cpp`):
```cpp
#include <Arduino.h>
#include "DebouncedButton.h"

DebouncedButton buttonPump1([] { return digitalRead(BUTTON_PUMP_1); });
DebouncedButton buttonCancel([] { return digitalRead(BUTTON_CANCEL); });
```

The lambda captures `digitalRead` at call time — no Arduino include needed inside `DebouncedButton.cpp`.

**In tests** (`test/test_main.cpp`):
```cpp
#include "DebouncedButton.h"
#include <unity.h>

void test_debounce_fires_once_after_stable_low() {
    int fakePin = HIGH;
    DebouncedButton btn([&] { return fakePin; }, /*debounceMs=*/50);

    // raw press — not yet stable
    fakePin = LOW;
    TEST_ASSERT_FALSE(btn.poll(0));   // t=0: change detected, timer starts
    TEST_ASSERT_FALSE(btn.poll(30));  // t=30: still within debounce window

    // stable for long enough
    TEST_ASSERT_TRUE(btn.poll(60));   // t=60: edge confirmed, fires once
    TEST_ASSERT_FALSE(btn.poll(70));  // t=70: does not fire again
}
```

No hardware required; the test runs in `env:native`.

## Debounce algorithm

This project uses the **timer-based approach** (Mellis / Lady Ada, 2006, Arduino official examples). On each call to `poll(nowMs)`:

1. Read the pin via the injected `PinReader`.
2. If the raw reading differs from the previous reading, record `lastChangeMs = nowMs`.
3. If the reading has been stable for longer than `debounceMs` **and** differs from the last confirmed state, update the confirmed state and return `true` if the new stable state is `LOW` (closed).

This is robust, easy to reason about, and maps naturally onto the existing `loop()`/`tick()` structure.

An alternative is the **shift-register approach** (Ganssle):

```cpp
state = (state << 1) | digitalRead(pin) | 0xfe00;
if (state == 0xff00) { /* stable LOW for 8 consecutive reads */ }
```

It needs no timestamp at all — just a consistent call rate — and is more compact. The downside is that the debounce window is implicitly set by `loop()` frequency rather than by an explicit millisecond threshold, which is harder to reason about and harder to test deterministically.

## Full HAL: virtual interfaces

For more complex hardware (I2C sensors, the ultrasonic enable line, the TOF sensor) a virtual interface is the stronger pattern:

```cpp
// IGpio.h  — pure interface, no Arduino dependency
struct IGpio {
    virtual int read(int pin) const = 0;
    virtual void write(int pin, int value) = 0;
    virtual void pinMode(int pin, int mode) = 0;
    virtual ~IGpio() = default;
};
```

Production code receives an `IGpio&`; tests inject a mock. This is the pattern used by [arduino_abstractions](https://github.com/mum4k/arduino_abstractions) (GoogleMock-based) and described in [Designing a HAL in C++](https://blog.mbedded.ninja/programming/languages/c-plus-plus/designing-a-hal-in-cpp/).

For simple GPIO like buttons and pump outputs, the `std::function` injection (above) is sufficient and avoids the boilerplate of a full virtual hierarchy.

## Layer summary

| Layer | File(s) | Arduino dependency | Testable on native |
|---|---|---|---|
| Core logic | `Giessanlage.h/.cpp` | None | Yes |
| Button debounce | `DebouncedButton.h/.cpp` | None (injected) | Yes |
| Hardware wiring | `main.cpp` | Yes (`Arduino.h`) | No |

## References

- David Mellis / Lady Ada — [Arduino debounce tutorial](https://docs.arduino.cc/built-in-examples/digital/Debounce/) (original timer-based approach)
- Jack Ganssle — [A Guide to Debouncing](http://www.ganssle.com/debouncing.htm) (shift-register approach)
- ElectronVector — [When you want to unit test... abstract the hardware](http://www.electronvector.com/blog/when-you-want-to-unit-test-abstract-the-hardware)
- mum4k — [arduino_abstractions](https://github.com/mum4k/arduino_abstractions) (virtual interface + GoogleMock pattern)
- mbedded.ninja — [Designing a HAL in C++](https://blog.mbedded.ninja/programming/languages/c-plus-plus/designing-a-hal-in-cpp/)
- e-tinkers — [The simplest button debounce solution](https://www.e-tinkers.com/2021/05/the-simplest-button-debounce-solution/)
