
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

    static const unsigned long INTERVAL_30S = 30UL * 1000UL;
    static const unsigned long INTERVAL_12H = 12UL * 60UL * 60UL * 1000UL;
    static const unsigned long INTERVAL_24H = 2UL * INTERVAL_12H;

    Giessanlage(
        unsigned long wateringTime = INTERVAL_24H,
        unsigned long pumpTimeCh1 = INTERVAL_30S,
        unsigned long pumpTimeCh2 = INTERVAL_30S);

    enum State : int
    {
        Undefined = 0,

        Idle,
        PumpingManual,
        PumpingAuto,
    };

    State getState(Channel channel) const;
    bool isPumping(Channel channel) const;
    bool allowStateChange(Channel channel, State newState) const;

    /// @brief central logic update loop
    /// @param delta time in ms since last update
    /// @return true if any channel changed state
    bool tick(unsigned long delta);

    /// @brief start a manual pump cycle on the given channel
    bool triggerPump(Channel channel);

    /// @brief stop pumping on the given channel
    bool stopPump(Channel channel);

    bool setPumpTime(Channel channel, unsigned long time);
    unsigned long getPumpTime(Channel channel) const;

    bool setWateringInterval(unsigned long time);
    unsigned long getWateringInterval() const;
    bool resetWateringTimer();

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

    static int idx(Channel c) { return static_cast<int>(c); }

    bool setState(Channel channel, State newState);
    bool allChannelsIdle() const;
    unsigned long maxPumpTime() const;

    void resetWateringTimerInternal();
    void resetPumpTimerInternal(Channel channel);
};

#endif
