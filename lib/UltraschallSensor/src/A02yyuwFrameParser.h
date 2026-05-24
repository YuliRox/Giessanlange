#ifndef A02YYUW_FRAME_PARSER_H
#define A02YYUW_FRAME_PARSER_H

#include <stdint.h>

class A02yyuwFrameParser
{
public:
    static constexpr uint8_t FRAME_HEADER = 0xFF;
    static constexpr uint8_t FRAME_SIZE = 4;

    bool consume(uint8_t byte)
    {
        if (framePos == 0U)
        {
            if (byte != FRAME_HEADER)
            {
                return false;
            }

            frame[framePos++] = byte;
            return false;
        }

        frame[framePos++] = byte;

        if (framePos < FRAME_SIZE)
        {
            return false;
        }

        framePos = 0U;

        const uint8_t checksum = static_cast<uint8_t>(frame[0] + frame[1] + frame[2]);
        if (checksum != frame[3])
        {
            valid = false;
            return false;
        }

        distanceCm = static_cast<uint16_t>((static_cast<uint16_t>(frame[1]) << 8) | frame[2]);
        valid = true;
        return true;
    }

    bool hasValidDistance() const
    {
        return valid;
    }

    uint16_t getDistanceCm() const
    {
        return distanceCm;
    }

    void clear()
    {
        framePos = 0U;
        valid = false;
        distanceCm = 0U;
    }

private:
    uint8_t frame[FRAME_SIZE] = {0U, 0U, 0U, 0U};
    uint8_t framePos = 0U;
    bool valid = false;
    uint16_t distanceCm = 0U;
};

#endif
