#ifndef DebouncedButton_h
#define DebouncedButton_h

#include <functional>

/// Platform-independent debounced button using injected pin readout.
///
/// The reader callable is expected to return Arduino-style HIGH (1) for
/// "open" and LOW (0) for "closed" (i.e. INPUT_PULLUP wiring). The class
/// is free of Arduino headers so it can be compiled and tested in the
/// native env; the caller provides a lambda wrapping digitalRead() on
/// hardware, or a stub returning a captured variable in tests.
class DebouncedButton
{
public:
    using PinReader = std::function<int()>;

    /// @param reader      callable returning the current raw pin level
    /// @param debounceMs  how long the pin must read stable before the
    ///                    edge is reported
    DebouncedButton(PinReader reader, unsigned long debounceMs = 100);

    /// Call every loop tick.
    /// @return true exactly once per confirmed closing edge (HIGH -> LOW).
    bool poll(unsigned long nowMs);

    /// Last confirmed stable state (1 = open, 0 = closed).
    bool isPressed() const;

private:
    PinReader _reader;
    unsigned long _debounceMs;
    int _state;
    int _lastReading;
    unsigned long _lastChangeMs;
};

#endif
