
#include "Giessanlage.h"

Giessanlage::Giessanlage(
    unsigned long wateringTime,
    unsigned long pumpTime) : state(State::Idle),
                              wateringTime(wateringTime),
                              pumpTime(pumpTime)
{
}

bool Giessanlage::allowStateChange(State newState) const
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

bool Giessanlage::setState(State newState)
{
    if (!allowStateChange(newState))
        return false;

    switch (newState)
    {
    case State::Idle:
        this->wateringTimer = this->wateringTime;
        break;

    case State::PumpingManual:
    case State::PumpingAuto:
        this->pumpTimer = this->pumpTime;
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

void Giessanlage::setPumpTime(unsigned long time)
{
    if (time <= 0)
        return;

    this->pumpTime = time;
}

unsigned long Giessanlage::getPumpTime() const
{
    return this->pumpTime;
}

void Giessanlage::setWateringInterval(unsigned long time)
{
    if (time - this->pumpTime <= 0UL)
        return;
    this->wateringTime = time;
    // reset current timer
    this->wateringTimer = wateringTime;
}

unsigned long Giessanlage::getWateringInterval() const
{
    return this->wateringTime;
}

unsigned long Giessanlage::getRemainingPumpTime() const
{
    return this->pumpTimer;
}

unsigned long Giessanlage::getRemainingWateringInterval() const
{
    return this->wateringTimer;
}

void updateTimer(unsigned long &timer, unsigned long delta)
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

bool Giessanlage::tick(unsigned long delta)
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
