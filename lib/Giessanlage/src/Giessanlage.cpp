
#include "Giessanlage.h"

Giessanlage::Giessanlage(
    unsigned long wateringTime,
    unsigned long pumpTimeCh1,
    unsigned long pumpTimeCh2)
{
    setWateringInterval(wateringTime);
    setPumpTime(Channel::One, pumpTimeCh1);
    setPumpTime(Channel::Two, pumpTimeCh2);
    setState(Channel::One, State::Idle);
    setState(Channel::Two, State::Idle);
}

bool Giessanlage::allChannelsIdle() const
{
    return channels[idx(Channel::One)].state == State::Idle &&
           channels[idx(Channel::Two)].state == State::Idle;
}

unsigned long Giessanlage::maxPumpTime() const
{
    const unsigned long a = channels[idx(Channel::One)].pumpTime;
    const unsigned long b = channels[idx(Channel::Two)].pumpTime;
    return (a > b) ? a : b;
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

bool Giessanlage::isAnyPumping() const
{
    return isPumping(Channel::One) || isPumping(Channel::Two);
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

bool Giessanlage::resetPumpTimer(Channel channel)
{
    resetPumpTimerInternal(channel);
    return true;
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

bool Giessanlage::tickChannel(Channel channel, unsigned long delta)
{
    ChannelData &ch = this->channels[idx(channel)];
    updateTimer(ch.pumpTimer, delta);

    switch (ch.state)
    {
    case State::Idle:
        if (this->wateringTimer == 0UL)
            return setState(channel, State::PumpingAuto);
        break;
    case State::PumpingManual:
    case State::PumpingAuto:
        if (ch.pumpTimer == 0UL)
            return setState(channel, State::Idle);
        break;
    default:
        break;
    }
    return false;
}

bool Giessanlage::tick(unsigned long delta)
{
    updateTimer(this->wateringTimer, delta);

    bool anyChange = false;
    anyChange |= tickChannel(Channel::One, delta);
    anyChange |= tickChannel(Channel::Two, delta);
    return anyChange;
}

bool Giessanlage::triggerAllPumps()
{
    const bool a = setState(Channel::One, State::PumpingManual);
    const bool b = setState(Channel::Two, State::PumpingManual);
    return a || b;
}

bool Giessanlage::triggerPump(Channel channel)
{
    return setState(channel, State::PumpingManual);
}

bool Giessanlage::stopAllPumps()
{
    const bool a = setState(Channel::One, State::Idle);
    const bool b = setState(Channel::Two, State::Idle);
    return a || b;
}

bool Giessanlage::stopPump(Channel channel)
{
    return setState(channel, State::Idle);
}
