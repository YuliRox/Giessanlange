
#ifndef Giessanlage_h
#define Giessanlage_h

class Giessanlage
{
public:
    static const int CHANNEL_COUNT = 2;

    static const unsigned long INTERVAL_30S = 30UL * 1000UL;
    static const unsigned long INTERVAL_12H = 12UL * 60UL * 60UL * 1000UL;
    static const unsigned long INTERVAL_24H = 2UL * INTERVAL_12H;

    Giessanlage(
        const unsigned long wateringTime = INTERVAL_24H,
        const unsigned long pumpTime = INTERVAL_30S);

    enum State : int
    {
        Undefined = 0,

        Idle,
        PumpingManual,
        PumpingAuto,
    };

    State getState(const int channel) const;

    /// @brief true if any channel is currently pumping
    bool isPumping() const;
    bool isPumping(const int channel) const;

    bool allowStateChange(const int channel, const State newState) const;

    /// @brief central logic update loop
    /// @param delta time in ms since last update
    /// @return true if any channel changed state
    bool tick(const unsigned long delta);

    /// @brief start a manual pump cycle on all idle channels
    /// @return true if at least one channel transitioned
    bool triggerPump();
    /// @brief start a manual pump cycle on the given channel
    bool triggerPump(const int channel);

    /// @brief stop pumping on all channels currently pumping
    /// @return true if at least one channel transitioned
    bool stopPump();
    /// @brief stop pumping on the given channel
    bool stopPump(const int channel);

    bool setPumpTime(const unsigned long time);
    unsigned long getPumpTime() const;

    bool setWateringInterval(const unsigned long time);
    unsigned long getWateringInterval() const;
    bool resetWateringTimer();

    unsigned long getRemainingPumpTime(const int channel) const;
    unsigned long getRemainingWateringInterval() const;

private:
    struct Channel
    {
        State state = State::Undefined;
        unsigned long pumpTimer = 0;
    };

    Channel channels[CHANNEL_COUNT];

    unsigned long wateringTime = 0;
    unsigned long pumpTime = 0;
    unsigned long wateringTimer = 0;

    bool setState(const int channel, const State newState);
    bool isValidChannel(const int channel) const;
    bool allChannelsIdle() const;

    void resetWateringTimerInternal();
    void resetPumpTimerInternal(const int channel);
};

#endif
