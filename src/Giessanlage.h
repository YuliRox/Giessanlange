
#ifndef Giessanlage_h
#define Giessanlage_h

class Giessanlage
{
public:
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

    State getState() const;
    /// @brief provides information if the external pump should be running
    /// @return true if pump should run 
    bool isPumping() const;

    bool allowStateChange(const State newState) const;
    
    /// @brief central logic update loop
    /// @param delta time in ms since last update
    /// @return true if there was a state change 
    bool tick(const unsigned long delta);

    /// @brief try to start a manual pumping process
    /// @note can only return true as long as the internal state is not pumping
    /// @return success status
    bool triggerPump();
    /// @brief try to stop the current pumping process
    /// @return success status. false if not pumping
    bool stopPump();
    /// @brief sets the time for how long the pump should be running
    /// @param time time in milliseconds
    /// @return success status. false if invalid time
    bool setPumpTime(const unsigned long time);
    unsigned long getPumpTime() const;
    /// @brief sets the time between automatic pumping
    /// @param time time in milliseconds
    /// @return success status. false if invalid time
    bool setWateringInterval(const unsigned long time);
    unsigned long getWateringInterval() const;
    /// @brief tries to reset a running watering interval timer
    /// @return success status. false if no timer is running
    bool resetWateringTimer();

    unsigned long getRemainingPumpTime() const;
    unsigned long getRemainingWateringInterval() const;

private:
    State state = State::Undefined;
    bool setState(const State newState);

    unsigned long wateringTime = 0;
    unsigned long pumpTime = 0;

    unsigned long wateringTimer = 0;
    unsigned long pumpTimer = 0;

    void resetWateringTimerInternal();
    void resetPumpTimerInternal();
};

#endif