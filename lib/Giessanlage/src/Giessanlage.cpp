
#include "Giessanlage.h"

Giessanlage::Giessanlage(
    unsigned long wateringTime,
    unsigned long pumpTimeCh1,
    unsigned long pumpTimeCh2)
    : wateringTime(INTERVAL_24H)
{
    channels[0].pumpTime = INTERVAL_30S;
    channels[1].pumpTime = INTERVAL_30S;
    setWateringInterval(wateringTime);
    setPumpTime(Channel::One, pumpTimeCh1);
    setPumpTime(Channel::Two, pumpTimeCh2);
    for (int c = 0; c < CHANNEL_COUNT; ++c)
        setState(static_cast<Channel>(c), State::Idle);
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

unsigned long Giessanlage::maxPumpTime() const
{
    unsigned long m = 0;
    for (int c = 0; c < CHANNEL_COUNT; ++c)
        if (this->channels[c].pumpTime > m)
            m = this->channels[c].pumpTime;
    return m;
}

bool Giessanlage::allowStateChange(Channel channel, const State newState) const
{
    switch (this->channels[idx(channel)].state)
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

bool Giessanlage::setState(Channel channel, const State newState)
{
    if (!allowStateChange(channel, newState))
        return false;

    switch (newState)
    {
    case State::Idle:
        this->channels[idx(channel)].pumpTimer = 0;
        this->channels[idx(channel)].state = newState;
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

    this->channels[idx(channel)].state = newState;
    return true;
}

Giessanlage::State Giessanlage::getState(Channel channel) const
{
    return this->channels[idx(channel)].state;
}

bool Giessanlage::isPumping(Channel channel) const
{
    const State s = this->channels[idx(channel)].state;
    return s == State::PumpingAuto || s == State::PumpingManual;
}

bool Giessanlage::setPumpTime(Channel channel, unsigned long time)
{
    if (time == 0UL)
        return false;

    this->channels[idx(channel)].pumpTime = time;
    return true;
}

unsigned long Giessanlage::getPumpTime(Channel channel) const
{
    return this->channels[idx(channel)].pumpTime;
}

void Giessanlage::resetPumpTimerInternal(Channel channel)
{
    this->channels[idx(channel)].pumpTimer = this->channels[idx(channel)].pumpTime;
}

bool Giessanlage::setWateringInterval(unsigned long time)
{
    if (time == 0UL)
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
    // Subtract the longest channel pump time from the watering interval so the
    // active-pump phase plus the idle phase together equal the configured
    // watering interval, regardless of which channel runs longest.
    const unsigned long pt = maxPumpTime();
    this->wateringTimer = (this->wateringTime > pt) ? (this->wateringTime - pt) : 0UL;
}

bool Giessanlage::resetWateringTimer()
{
    if (this->wateringTimer == 0UL)
        return false;

    resetWateringTimerInternal();
    return true;
}

unsigned long Giessanlage::getRemainingPumpTime(Channel channel) const
{
    return this->channels[idx(channel)].pumpTimer;
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

bool Giessanlage::tick(unsigned long delta)
{
    updateTimer(this->wateringTimer, delta);

    bool anyChange = false;
    for (int c = 0; c < CHANNEL_COUNT; ++c)
    {
        updateTimer(this->channels[c].pumpTimer, delta);

        const Channel ch = static_cast<Channel>(c);
        switch (this->channels[c].state)
        {
        case State::Idle:
            if (this->wateringTimer == 0UL)
                anyChange |= setState(ch, State::PumpingAuto);
            break;
        case State::PumpingManual:
        case State::PumpingAuto:
            if (this->channels[c].pumpTimer == 0UL)
                anyChange |= setState(ch, State::Idle);
            break;
        default:
            break;
        }
    }
    return anyChange;
}

bool Giessanlage::triggerPump(Channel channel)
{
    return setState(channel, State::PumpingManual);
}

bool Giessanlage::stopPump(Channel channel)
{
    return setState(channel, State::Idle);
}
