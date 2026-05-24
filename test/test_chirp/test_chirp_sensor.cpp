#include <unity.h>

#include "ChirpSensorCore.h"

void test_chirp_core_moisture_requires_init()
{
    ChirpSensorCore core;
    uint16_t value = 0U;
    TEST_ASSERT_FALSE(core.convertMoisture(300, value));
}

void test_chirp_core_moisture_rejects_negative()
{
    ChirpSensorCore core;
    core.setInitialized(true);
    uint16_t value = 0U;
    TEST_ASSERT_FALSE(core.convertMoisture(-1, value));
}

void test_chirp_core_moisture_accepts_valid_value()
{
    ChirpSensorCore core;
    core.setInitialized(true);
    uint16_t value = 0U;
    TEST_ASSERT_TRUE(core.convertMoisture(345, value));
    TEST_ASSERT_EQUAL_UINT16(345U, value);
}

void test_chirp_core_temperature_requires_init()
{
    ChirpSensorCore core;
    float temp = 0.0F;
    TEST_ASSERT_FALSE(core.convertTemperatureDeciC(215, temp));
}

void test_chirp_core_temperature_scales_deci_c()
{
    ChirpSensorCore core;
    core.setInitialized(true);
    float temp = 0.0F;
    TEST_ASSERT_TRUE(core.convertTemperatureDeciC(215, temp));
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 21.5F, temp);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_chirp_core_moisture_requires_init);
    RUN_TEST(test_chirp_core_moisture_rejects_negative);
    RUN_TEST(test_chirp_core_moisture_accepts_valid_value);
    RUN_TEST(test_chirp_core_temperature_requires_init);
    RUN_TEST(test_chirp_core_temperature_scales_deci_c);
    return UNITY_END();
}
