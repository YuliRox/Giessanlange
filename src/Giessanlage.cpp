
#include "Giessanlage.h"

Giessanlage::Giessanlage(
    const unsigned long wateringTime,
    const unsigned long pumpTime) : state(State::Undefined),
                                             wateringTime(INTERVAL_24H),
                                             pumpTime(INTERVAL_30S)
{
    setWateringInterval(wateringTime);
    setPumpTime(pumpTime);
    setState(State::Idle);
}

bool Giessanlage::allowStateChange(const State newState) const
{
    switch (this->state)
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

bool Giessanlage::setState(const State newState)
{
    if (!allowStateChange(newState))
        return false;

    switch (newState)
    {
    case State::Idle:
        this->resetWateringTimerInternal();
        this->pumpTimer = 0;
        break;

    case State::PumpingAuto:
        this->wateringTimer = 0;
    case State::PumpingManual:
        this->resetPumpTimerInternal();
        break;

    default:
        break;
    }

    this->state = newState;
    return true;
}

Giessanlage::State Giessanlage::getState() const
{
    return this->state;
}

bool Giessanlage::isPumping() const
{
    return this->state == State::PumpingAuto || this->state == State::PumpingManual;
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

void Giessanlage::resetPumpTimerInternal()
{
    this->pumpTimer = this->pumpTime;
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
    // substract pump time from watering interval to not shift the timer logic as only one timer activly runs
    this->wateringTimer = this->wateringTime - this->pumpTime;
}

bool Giessanlage::resetWateringTimer()
{
    if (this->wateringTimer <= 0UL)
        return false;

    resetWateringTimerInternal();

    return true;
}

unsigned long Giessanlage::getRemainingPumpTime() const
{
    return this->pumpTimer;
}

unsigned long Giessanlage::getRemainingWateringInterval() const
{
    return this->wateringTimer;
}

void updateTimer(unsigned long &timer, const unsigned long delta)
{
    if (timer > delta)
    {
        timer -= delta;
    }
    else if (timer <= delta)
    {
        timer = 0;
    }
}

bool Giessanlage::tick(const unsigned long delta)
{
    // scheduled watering
    updateTimer(this->wateringTimer, delta);
    // pump timer
    updateTimer(this->pumpTimer, delta);

    switch (this->state)
    {
    case State::Idle:
        if (this->wateringTimer <= 0UL)
            return this->setState(State::PumpingAuto);
        return false;
    case State::PumpingManual:
    case State::PumpingAuto:
        if (this->pumpTimer <= 0UL)
            return this->setState(State::Idle);
        return false;

    default:
        return false;
    }
}

bool Giessanlage::triggerPump()
{
    return setState(State::PumpingManual);
}

bool Giessanlage::stopPump()
{
    return setState(State::Idle);
}
