#ifndef WifiManager_h
#define WifiManager_h

#include <functional>
#include <string>

/// Platform-independent WiFi reconnect state machine. Drives an injected
/// "begin connection attempt" callable and an "is connected" probe; never
/// blocks. The actual `WiFi.begin()` / `WiFi.status()` calls live in the
/// Arduino-side wiring, behind these injections, so this class compiles
/// in the native env and is unit-testable.
class WifiManager
{
public:
    enum class State : int
    {
        Disconnected = 0,
        Connecting,
        Connected,
    };

    /// Called once per tick to advance the state machine. The class hands
    /// the SSID + password back to the Arduino side via beginConnect; the
    /// Arduino side calls `WiFi.begin(ssid, pass)`.
    using BeginConnectFn = std::function<void(const std::string &ssid,
                                              const std::string &pass)>;

    /// Polled on every tick. Returns true once the radio reports
    /// associated; the manager uses this to transition from Connecting
    /// to Connected.
    using IsConnectedFn = std::function<bool()>;

    struct Config
    {
        /// Initial retry backoff after a failed attempt, in ms.
        unsigned long initialBackoffMs = 2000UL;
        /// Maximum backoff cap, in ms. Doubles after each failed attempt
        /// until it reaches the cap.
        unsigned long maxBackoffMs = 30UL * 1000UL;
        /// How long a single connection attempt is given before being
        /// declared a failure and the backoff applied. ESP32-C6 WiFi
        /// usually associates within ~5 s on a healthy AP.
        unsigned long connectTimeoutMs = 10UL * 1000UL;
    };

    WifiManager(BeginConnectFn beginConnect,
                IsConnectedFn isConnectedProbe,
                const std::string &ssid,
                const std::string &password,
                Config config);

    WifiManager(BeginConnectFn beginConnect,
                IsConnectedFn isConnectedProbe,
                const std::string &ssid,
                const std::string &password)
        : WifiManager(std::move(beginConnect),
                      std::move(isConnectedProbe),
                      ssid, password, Config{}) {}

    /// Drive the state machine. Pass elapsed ms since the previous call.
    /// Returns true if the state changed during this tick.
    bool tick(unsigned long deltaMs);

    State state() const;
    bool isConnected() const;

    /// For diagnostics: how many failed attempts since the last successful
    /// association. Resets to 0 on Connected.
    unsigned int failedAttempts() const;

private:
    BeginConnectFn _beginConnect;
    IsConnectedFn _isConnectedProbe;
    std::string _ssid;
    std::string _password;
    Config _config;

    State _state = State::Disconnected;
    unsigned long _elapsedInState = 0;
    unsigned long _nextBackoffMs;
    unsigned int _failedAttempts = 0;

    bool transitionTo(State next);
};

#endif
