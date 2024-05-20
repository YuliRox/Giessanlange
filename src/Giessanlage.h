
#ifndef Giessanlage_h
#define Giessanlage_h

class Giessanlage
{
public:
    static const unsigned long INTERVAL_30S = 30UL * 1000UL;
    static const unsigned long INTERVAL_12H = 12UL * 60UL * 60UL * 1000UL;
    static const unsigned long INTERVAL_24H = 2UL * INTERVAL_12H;

    Giessanlage(
        unsigned long wateringTime = INTERVAL_24H,
        unsigned long pumpTime = INTERVAL_30S);

    enum State : int
    {
        Undefined = 0,

        Idle,
        PumpingManual,
        PumpingAuto,
    };

    State getState() const;
    bool isPumping() const;

    bool allowStateChange(State newState) const;
    
    /// @brief central logic update loop
    /// @param delta time in ms since last update
    /// @return true if there was a state change 
    bool tick(unsigned long delta);

    bool triggerPump();
    bool stopPump();

    void setPumpTime(unsigned long time);
    unsigned long getPumpTime() const;
    void setWateringInterval(unsigned long time);
    unsigned long getWateringInterval() const;

    unsigned long getRemainingPumpTime() const;
    unsigned long getRemainingWateringInterval() const;

private:
    State state = State::Undefined;
    bool setState(State newState);

    unsigned long wateringTime = 0;
    unsigned long pumpTime = 0;

    unsigned long wateringTimer = 0;
    unsigned long pumpTimer = 0;
};

#endif