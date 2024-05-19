
#ifndef Giessanlage_h
#define Giessanlage_h

class Giessanlage
{
public:
    static const unsigned long INTERVAL_12H = 12 * 60 * 60 * 1000;
    static const unsigned long INTERVAL_24H = 2 * INTERVAL_12H;

    Giessanlage(
        unsigned long wateringTime = INTERVAL_24H,
        unsigned long pumpTime = 30 * 1000);

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
    void tick(unsigned long delta);

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

    unsigned long pumpTime = 0;
    unsigned long wateringTime = 0;

    unsigned long wateringTimer = 0;
    unsigned long pumpTimer = 0;
};

#endif