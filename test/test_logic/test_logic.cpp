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

// Regression: stopping one auto-pumping channel while another is still pumping
// must not leave wateringTimer at 0 and immediately re-trigger PumpingAuto on
// the stopped channel on the next tick.
void test_per_channel_stop_does_not_immediately_retrigger_auto()
{
    Giessanlage g(1000UL, 200UL);

    // Drive both channels into PumpingAuto by elapsing the watering interval.
    g.tick(800UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(0));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(1));

    // Stop only channel 0 while channel 1 is still pumping.
    TEST_ASSERT_TRUE(g.stopPump(0));
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(0));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(1));

    // Next tick: channel 0 must stay Idle, not re-enter PumpingAuto.
    g.tick(10UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(0));
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
    RUN_TEST(test_per_channel_stop_does_not_immediately_retrigger_auto);
    return UNITY_END();
}
