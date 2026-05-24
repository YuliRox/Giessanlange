#include "EInkDisplay.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

bool EInkDisplay::begin()
{
    initialized = true;
    sleeping = false;
    refillDirty = true;
    regularDirty = true;
    return true;
}

void EInkDisplay::setMode(const Mode nextMode)
{
    if (mode == nextMode)
    {
        return;
    }

    mode = nextMode;
    if (mode == Mode::Refill)
    {
        refillDirty = true;
    }
    else
    {
        regularDirty = true;
    }
}

EInkDisplay::Mode EInkDisplay::getMode() const
{
    return mode;
}

void EInkDisplay::updateRefill(const RefillState &state)
{
    latestRefillState = state;
    refillDirty = true;
}

void EInkDisplay::updateRegular(const RegularState &state)
{
    latestRegularState = state;
    regularDirty = true;
}

void EInkDisplay::tick(const uint32_t nowMs)
{
    if (!initialized || sleeping)
    {
        return;
    }

    if (mode == Mode::Refill)
    {
        const bool intervalReached = (nowMs - lastRefillRenderMs) >= minRefillRefreshMs;
        if (refillDirty && intervalReached)
        {
            renderRefill(nowMs);
            refillDirty = false;
            lastRefillRenderMs = nowMs;
        }
        return;
    }

    const bool intervalReached = (nowMs - lastRegularRenderMs) >= regularRefreshMs;
    if (regularDirty || intervalReached)
    {
        renderRegular(nowMs);
        regularDirty = false;
        lastRegularRenderMs = nowMs;
    }
}

void EInkDisplay::sleep()
{
    sleeping = true;
}

void EInkDisplay::setRefillStopThresholdMm(const uint16_t thresholdMm)
{
    refillStopThresholdMm = thresholdMm;
}

uint16_t EInkDisplay::getRefillStopThresholdMm() const
{
    return refillStopThresholdMm;
}

void EInkDisplay::setMinRefillRefreshMs(const uint32_t intervalMs)
{
    if (intervalMs > 0UL)
    {
        minRefillRefreshMs = intervalMs;
    }
}

void EInkDisplay::setRegularRefreshMs(const uint32_t intervalMs)
{
    if (intervalMs > 0UL)
    {
        regularRefreshMs = intervalMs;
    }
}

uint32_t EInkDisplay::getLastRefillRenderMs() const
{
    return lastRefillRenderMs;
}

uint32_t EInkDisplay::getLastRegularRenderMs() const
{
    return lastRegularRenderMs;
}

bool EInkDisplay::hasPendingRefillFrame() const
{
    return refillDirty;
}

bool EInkDisplay::hasPendingRegularFrame() const
{
    return regularDirty;
}

bool EInkDisplay::isNearFull(const RefillState &state) const
{
    return state.sensorValid && state.distanceMm <= refillStopThresholdMm;
}

void EInkDisplay::renderRefill(const uint32_t nowMs)
{
#ifdef ARDUINO
    Serial.print("[EInk Refill] t=");
    Serial.print(nowMs);
    Serial.print("ms distance=");
    Serial.print(latestRefillState.distanceMm);
    Serial.print("mm valid=");
    Serial.print(latestRefillState.sensorValid ? "yes" : "no");

    if (isNearFull(latestRefillState))
    {
        Serial.print(" RED:STOP");
    }

    Serial.println();
#else
    (void)nowMs;
#endif
}

void EInkDisplay::renderRegular(const uint32_t nowMs)
{
#ifdef ARDUINO
    Serial.print("[EInk Regular] t=");
    Serial.print(nowMs);
    Serial.print("ms level=");
    Serial.print(static_cast<unsigned int>(latestRegularState.levelPercent));
    Serial.print("% valid=");
    Serial.print(latestRegularState.levelValid ? "yes" : "no");
    Serial.print(" nextP1=");
    Serial.print(latestRegularState.nextPump1Ms / 1000UL);
    Serial.print("s nextP2=");
    Serial.print(latestRegularState.nextPump2Ms / 1000UL);
    Serial.println("s");
#else
    (void)nowMs;
#endif
}
