
#ifndef Giessanlage_h
#define Giessanlage_h

class Giessanlage
{
public:
    enum class Channel : int
    {
        One = 0,
        Two = 1,
    };

    static const unsigned long INTERVAL_02M = 2UL * 60UL * 1000UL;
    static const unsigned long INTERVAL_01M = 60UL * 1000UL;
    static const unsigned long INTERVAL_30S = 30UL * 1000UL;
    static const unsigned long INTERVAL_12H = 12UL * 60UL * 60UL * 1000UL;
    static const unsigned long INTERVAL_24H = 2UL * INTERVAL_12H;

    Giessanlage(
        unsigned long wateringTime = INTERVAL_12H,
        unsigned long pumpTimeCh1 = INTERVAL_02M,
        unsigned long pumpTimeCh2 = INTERVAL_02M);

    enum State : int
    {
        Undefined = 0,

        Idle,
        PumpingManual,
        PumpingAuto,
    };

    State getState(Channel channel) const;

    /// @brief true if any channel is currently pumping
    bool isAnyPumping() const;
    bool isPumping(Channel channel) const;

    bool allowStateChange(Channel channel, State newState) const;

    /// @brief central logic update loop
    /// @param delta time in ms since last update
    /// @return true if any channel changed state
    bool tick(unsigned long delta);

    /// @brief start a manual pump cycle on all idle channels
    /// @return true if at least one channel transitioned
    bool triggerAllPumps();
    /// @brief start a manual pump cycle on the given channel
    bool triggerPump(Channel channel);

    /// @brief stop pumping on all channels currently pumping
    /// @return true if at least one channel transitioned
    bool stopAllPumps();
    /// @brief stop pumping on the given channel
    bool stopPump(Channel channel);

    bool setPumpTime(Channel channel, unsigned long time);
    unsigned long getPumpTime(Channel channel) const;

    bool setWateringInterval(unsigned long time);
    unsigned long getWateringInterval() const;
    bool resetWateringTimer();

    /// @brief suppress automatic watering without touching the configuration
    ///
    /// While paused, no channel may transition Idle -> PumpingAuto. Manual
    /// control is deliberately unaffected: triggerPump / triggerAllPumps and
    /// the physical buttons keep working, because pausing means "do not water
    /// on your own", not "refuse to work" — during maintenance one wants to
    /// run a pump by hand precisely while automatic watering is off. A
    /// PumpingAuto cycle already in flight is allowed to finish.
    ///
    /// The watering timer keeps running while paused; each expiry is consumed
    /// and the timer rearmed (see tick()), so resuming never releases a
    /// backlogged cycle and watering continues on its normal cadence.
    /// @return true if the flag changed
    bool setPaused(bool paused);
    bool isPaused() const;

    /// @brief restart the in-flight pump countdown to the current pumpTime,
    /// without changing state (unlike triggerPump/stopPump)
    bool resetPumpTimer(Channel channel);

    unsigned long getRemainingPumpTime(Channel channel) const;
    unsigned long getRemainingWateringInterval() const;

private:
    static constexpr int CHANNEL_COUNT = 2;

    struct ChannelData
    {
        State state = State::Undefined;
        unsigned long pumpTime = 0;
        unsigned long pumpTimer = 0;
    };

    ChannelData channels[CHANNEL_COUNT];

    unsigned long wateringTime = 0;
    unsigned long wateringTimer = 0;

    // Suppresses Idle -> PumpingAuto only; see setPaused().
    bool paused = false;

    static int idx(Channel c) { return static_cast<int>(c); }

    bool setState(Channel channel, State newState);
    bool allChannelsIdle() const;
    unsigned long maxPumpTime() const;
    bool tickChannel(Channel channel, unsigned long delta);

    void resetWateringTimerInternal();
    void resetPumpTimerInternal(Channel channel);
};

#endif
