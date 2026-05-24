#include <unity.h>

#include "Giessanlage.h"

void test_defaults_are_initialized()
{
    Giessanlage g;
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState());
    TEST_ASSERT_EQUAL_UINT32(Giessanlage::INTERVAL_24H, g.getWateringInterval());
    TEST_ASSERT_EQUAL_UINT32(Giessanlage::INTERVAL_30S, g.getPumpTime());
}

void test_trigger_and_stop_pump()
{
    Giessanlage g;
    TEST_ASSERT_TRUE(g.triggerPump());
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState());
    TEST_ASSERT_TRUE(g.isPumping());

    TEST_ASSERT_TRUE(g.stopPump());
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState());
    TEST_ASSERT_FALSE(g.isPumping());
}

void test_auto_pump_after_interval_elapsed()
{
    Giessanlage g(1000UL, 200UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState());

    // Internally, the idle timer is initialized to wateringTime - pumpTime.
    // So auto-pump should start after 800 ms for (1000, 200).
    g.tick(800UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState());
    TEST_ASSERT_TRUE(g.isPumping());
}

void test_pump_stops_after_pump_time_elapsed()
{
    Giessanlage g(1000UL, 200UL);
    TEST_ASSERT_TRUE(g.triggerPump());
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState());

    g.tick(150UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState());

    g.tick(50UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState());
    TEST_ASSERT_FALSE(g.isPumping());
}

void test_setters_reject_invalid_values()
{
    Giessanlage g;
    TEST_ASSERT_FALSE(g.setPumpTime(0UL));
    TEST_ASSERT_FALSE(g.setWateringInterval(0UL));
}

void test_cannot_trigger_twice_while_pumping()
{
    Giessanlage g;
    TEST_ASSERT_TRUE(g.triggerPump());
    TEST_ASSERT_FALSE(g.triggerPump());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults_are_initialized);
    RUN_TEST(test_trigger_and_stop_pump);
    RUN_TEST(test_auto_pump_after_interval_elapsed);
    RUN_TEST(test_pump_stops_after_pump_time_elapsed);
    RUN_TEST(test_setters_reject_invalid_values);
    RUN_TEST(test_cannot_trigger_twice_while_pumping);
    return UNITY_END();
}
