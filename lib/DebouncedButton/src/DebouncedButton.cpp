#include "DebouncedButton.h"

namespace
{
constexpr int OPEN = 1;   // HIGH
constexpr int CLOSED = 0; // LOW
} // namespace

DebouncedButton::DebouncedButton(PinReader reader, unsigned long debounceMs)
    : _reader(std::move(reader)),
      _debounceMs(debounceMs),
      _state(OPEN),
      _lastReading(OPEN),
      _lastChangeMs(0)
{
}

bool DebouncedButton::poll(unsigned long nowMs)
{
    const int reading = _reader();
    if (reading != _lastReading)
        _lastChangeMs = nowMs;
    _lastReading = reading;

    if ((nowMs - _lastChangeMs) > _debounceMs && reading != _state)
    {
        _state = reading;
        return _state == CLOSED;
    }
    return false;
}

bool DebouncedButton::isPressed() const
{
    return _state == CLOSED;
}
