
#include "Giessanlage.h"

Giessanlage::Giessanlage(
    const unsigned long wateringTime,
    const unsigned long pumpTime) : wateringTime(INTERVAL_24H),
                                    pumpTime(INTERVAL_30S)
{
    setWateringInterval(wateringTime);
    setPumpTime(pumpTime);
    for (int c = 0; c < CHANNEL_COUNT; ++c)
    {
        setState(c, State::Idle);
    }
}

bool Giessanlage::isValidChannel(const int channel) const
{
    return channel >= 0 && channel < CHANNEL_COUNT;
}

bool Giessanlage::allChannelsIdle() const
{
    for (int c = 0; c < CHANNEL_COUNT; ++c)
    {
        if (this->channels[c].state != State::Idle)
            return false;
    }
    return true;
}

bool Giessanlage::allowStateChange(const int channel, const State newState) const
{
    if (!isValidChannel(channel))
        return false;

    switch (this->channels[channel].state)
    {
    case State::Undefined:
        return (newState == Idle);

    case State::Idle:
        return (newState == State::PumpingManual || newState == State::PumpingAuto);

    case State::PumpingManual:
    case State::PumpingAuto:
        return (newState == Idle);

    default:
        return false;
    }
}

bool Giessanlage::setState(const int channel, const State newState)
{
    if (!allowStateChange(channel, newState))
        return false;

    switch (newState)
    {
    case State::Idle:
        this->channels[channel].pumpTimer = 0;
        this->channels[channel].state = newState;
        // Reset the shared watering timer on every Idle transition; otherwise a
        // per-channel stop while another channel is still pumping would leave
        // wateringTimer at 0 and immediately re-trigger PumpingAuto on the
        // next tick.
        this->resetWateringTimerInternal();
        return true;

    case State::PumpingAuto:
        this->wateringTimer = 0;
        // fallthrough
    case State::PumpingManual:
        this->resetPumpTimerInternal(channel);
        break;

    default:
        break;
    }

    this->channels[channel].state = newState;
    return true;
}

Giessanlage::State Giessanlage::getState(const int channel) const
{
    if (!isValidChannel(channel))
        return State::Undefined;
    return this->channels[channel].state;
}

bool Giessanlage::isPumping() const
{
    for (int c = 0; c < CHANNEL_COUNT; ++c)
    {
        if (isPumping(c))
            return true;
    }
    return false;
}

bool Giessanlage::isPumping(const int channel) const
{
    if (!isValidChannel(channel))
        return false;
    const State s = this->channels[channel].state;
    return s == State::PumpingAuto || s == State::PumpingManual;
}

bool Giessanlage::setPumpTime(const unsigned long time)
{
    if (time <= 0)
        return false;

    this->pumpTime = time;
    return true;
}

unsigned long Giessanlage::getPumpTime() const
{
    return this->pumpTime;
}

void Giessanlage::resetPumpTimerInternal(const int channel)
{
    this->channels[channel].pumpTimer = this->pumpTime;
}

bool Giessanlage::setWateringInterval(const unsigned long time)
{
    if (time <= 0UL)
        return false;

    this->wateringTime = time;
    return true;
}

unsigned long Giessanlage::getWateringInterval() const
{
    return this->wateringTime;
}

void Giessanlage::resetWateringTimerInternal()
{
    // subtract pump time from watering interval so pump-time and idle-time
    // together equal the configured watering interval
    this->wateringTimer = this->wateringTime - this->pumpTime;
}

bool Giessanlage::resetWateringTimer()
{
    if (this->wateringTimer <= 0UL)
        return false;

    resetWateringTimerInternal();
    return true;
}

unsigned long Giessanlage::getRemainingPumpTime(const int channel) const
{
    if (!isValidChannel(channel))
        return 0;
    return this->channels[channel].pumpTimer;
}

unsigned long Giessanlage::getRemainingWateringInterval() const
{
    return this->wateringTimer;
}

static void updateTimer(unsigned long &timer, const unsigned long delta)
{
    if (timer > delta)
        timer -= delta;
    else
        timer = 0;
}

bool Giessanlage::tick(const unsigned long delta)
{
    updateTimer(this->wateringTimer, delta);

    bool anyChange = false;
    for (int c = 0; c < CHANNEL_COUNT; ++c)
    {
        updateTimer(this->channels[c].pumpTimer, delta);

        switch (this->channels[c].state)
        {
        case State::Idle:
            if (this->wateringTimer <= 0UL)
                anyChange |= setState(c, State::PumpingAuto);
            break;
        case State::PumpingManual:
        case State::PumpingAuto:
            if (this->channels[c].pumpTimer <= 0UL)
                anyChange |= setState(c, State::Idle);
            break;
        default:
            break;
        }
    }
    return anyChange;
}

bool Giessanlage::triggerPump()
{
    bool any = false;
    for (int c = 0; c < CHANNEL_COUNT; ++c)
        any |= setState(c, State::PumpingManual);
    return any;
}

bool Giessanlage::triggerPump(const int channel)
{
    return setState(channel, State::PumpingManual);
}

bool Giessanlage::stopPump()
{
    bool any = false;
    for (int c = 0; c < CHANNEL_COUNT; ++c)
        any |= setState(c, State::Idle);
    return any;
}

bool Giessanlage::stopPump(const int channel)
{
    return setState(channel, State::Idle);
}
