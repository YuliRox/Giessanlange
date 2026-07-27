#ifndef EINK_DISPLAY_H
#define EINK_DISPLAY_H

#include <stdint.h>

class EInkDisplay
{
public:
    enum class Mode : uint8_t
    {
        Refill = 0,
        Regular = 1,
    };

    struct RefillState
    {
        uint16_t distanceMm = 0U;
        bool sensorValid = false;
        uint32_t sampleTimestampMs = 0UL;
    };

    struct RegularState
    {
        uint8_t levelPercent = 0U;
        bool levelValid = false;
        uint32_t nextPump1Ms = 0UL;
        uint32_t nextPump2Ms = 0UL;
    };

    bool begin();
    void setMode(Mode mode);
    Mode getMode() const;

    void updateRefill(const RefillState &state);
    void updateRegular(const RegularState &state);
    void tick(uint32_t nowMs);
    void sleep();

    void setRefillStopThresholdMm(uint16_t thresholdMm);
    uint16_t getRefillStopThresholdMm() const;

    void setMinRefillRefreshMs(uint32_t intervalMs);
    void setRegularRefreshMs(uint32_t intervalMs);
    uint32_t getLastRefillRenderMs() const;
    uint32_t getLastRegularRenderMs() const;
    bool hasPendingRefillFrame() const;
    bool hasPendingRegularFrame() const;

private:
    bool initialized = false;
    bool sleeping = false;
    Mode mode = Mode::Regular;

    uint16_t refillStopThresholdMm = 20U;
    uint32_t minRefillRefreshMs = 5000UL;
    uint32_t regularRefreshMs = 15UL * 60UL * 1000UL;

    uint32_t lastRefillRenderMs = 0UL;
    uint32_t lastRegularRenderMs = 0UL;

    bool refillDirty = false;
    bool regularDirty = false;

    RefillState latestRefillState;
    RegularState latestRegularState;

    void renderRefill(uint32_t nowMs);
    void renderRegular(uint32_t nowMs);
    bool isNearFull(const RefillState &state) const;
};

#endif
