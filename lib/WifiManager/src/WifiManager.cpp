#include "WifiManager.h"

WifiManager::WifiManager(BeginConnectFn beginConnect,
                         IsConnectedFn isConnectedProbe,
                         const std::string &ssid,
                         const std::string &password,
                         Config config)
    : _beginConnect(std::move(beginConnect)),
      _isConnectedProbe(std::move(isConnectedProbe)),
      _ssid(ssid),
      _password(password),
      _config(config),
      _nextBackoffMs(config.initialBackoffMs)
{
}

bool WifiManager::tick(unsigned long deltaMs)
{
    // Operating without credentials: stay parked in Disconnected, never
    // call WiFi.begin. Lets the device run pumps fine on a freshly
    // flashed unit before secrets.ini is filled in.
    if (_ssid.empty())
        return false;

    _elapsedInState += deltaMs;

    switch (_state)
    {
    case State::Disconnected:
        if (_elapsedInState >= _nextBackoffMs)
        {
            _beginConnect(_ssid, _password);
            return transitionTo(State::Connecting);
        }
        return false;

    case State::Connecting:
        if (_isConnectedProbe())
        {
            _failedAttempts = 0;
            _nextBackoffMs = _config.initialBackoffMs;
            return transitionTo(State::Connected);
        }
        if (_elapsedInState >= _config.connectTimeoutMs)
        {
            ++_failedAttempts;
            _nextBackoffMs *= 2;
            if (_nextBackoffMs > _config.maxBackoffMs)
                _nextBackoffMs = _config.maxBackoffMs;
            return transitionTo(State::Disconnected);
        }
        return false;

    case State::Connected:
        if (!_isConnectedProbe())
            return transitionTo(State::Disconnected);
        return false;
    }
    return false;
}

WifiManager::State WifiManager::state() const
{
    return _state;
}

bool WifiManager::isConnected() const
{
    return _state == State::Connected;
}

unsigned int WifiManager::failedAttempts() const
{
    return _failedAttempts;
}

bool WifiManager::transitionTo(State next)
{
    if (_state == next)
        return false;
    _state = next;
    _elapsedInState = 0;
    return true;
}
