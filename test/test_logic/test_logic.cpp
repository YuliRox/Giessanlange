#include <unity.h>

#include "Giessanlage.h"

using Channel = Giessanlage::Channel;

void test_defaults_are_initialized()
{
    Giessanlage g;
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::Two));
    TEST_ASSERT_EQUAL_UINT32(Giessanlage::INTERVAL_12H, g.getWateringInterval());
    TEST_ASSERT_EQUAL_UINT32(Giessanlage::INTERVAL_02M, g.getPumpTime(Channel::One));
    TEST_ASSERT_EQUAL_UINT32(Giessanlage::INTERVAL_02M, g.getPumpTime(Channel::Two));
}

void test_trigger_and_stop_pump()
{
    Giessanlage g;
    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState(Channel::One));
    TEST_ASSERT_TRUE(g.isPumping(Channel::One));

    TEST_ASSERT_TRUE(g.stopPump(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));
    TEST_ASSERT_FALSE(g.isPumping(Channel::One));
}

void test_auto_pump_after_interval_elapsed()
{
    Giessanlage g(1000UL, 200UL, 200UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));

    // Idle timer is initialized to wateringTime - max(pumpTime per channel),
    // so auto-pump should start after 800 ms for (1000, 200, 200).
    g.tick(800UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(Channel::Two));
    TEST_ASSERT_TRUE(g.isPumping(Channel::One));
}

void test_pump_stops_after_pump_time_elapsed()
{
    Giessanlage g(1000UL, 200UL, 200UL);
    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState(Channel::One));

    g.tick(150UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState(Channel::One));

    g.tick(50UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));
    TEST_ASSERT_FALSE(g.isPumping(Channel::One));
}

void test_setters_reject_invalid_values()
{
    Giessanlage g;
    TEST_ASSERT_FALSE(g.setPumpTime(Channel::One, 0UL));
    TEST_ASSERT_FALSE(g.setPumpTime(Channel::Two, 0UL));
    TEST_ASSERT_FALSE(g.setWateringInterval(0UL));
}

void test_cannot_trigger_twice_while_pumping()
{
    Giessanlage g;
    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    TEST_ASSERT_FALSE(g.triggerPump(Channel::One));
}

// Regression: stopping one auto-pumping channel while another is still pumping
// must not leave wateringTimer at 0 and immediately re-trigger PumpingAuto on
// the stopped channel on the next tick.
void test_per_channel_stop_does_not_immediately_retrigger_auto()
{
    Giessanlage g(1000UL, 200UL, 200UL);

    // Drive both channels into PumpingAuto by elapsing the watering interval.
    g.tick(800UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(Channel::Two));

    // Stop only channel 1 while channel 2 is still pumping.
    TEST_ASSERT_TRUE(g.stopPump(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingAuto, g.getState(Channel::Two));

    // Next tick: channel 1 must stay Idle, not re-enter PumpingAuto.
    g.tick(10UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));
}

// Each channel honors its own configured pump duration, independent of the
// other. With pumpTime ch1=100, ch2=300, after 150 ms ch1 must be Idle while
// ch2 keeps pumping; ch2 stops at 300 ms.
void test_per_channel_pump_durations_are_independent()
{
    Giessanlage g(10000UL, 100UL, 300UL);

    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    TEST_ASSERT_TRUE(g.triggerPump(Channel::Two));

    g.tick(150UL);
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::One));
    TEST_ASSERT_EQUAL(Giessanlage::State::PumpingManual, g.getState(Channel::Two));

    g.tick(160UL); // total elapsed for ch2 = 310 ms > 300
    TEST_ASSERT_EQUAL(Giessanlage::State::Idle, g.getState(Channel::Two));
}

// Regression: an MQTT config update applies the new pump/watering values via
// setPumpTime()/setWateringInterval(), then must call resetWateringTimer() —
// mirroring src/mqtt_glue.cpp's apply callback — or the in-flight countdown
// keeps running against the stale interval instead of the newly configured one.
void test_config_update_resets_watering_timer()
{
    Giessanlage g(1000UL, 200UL, 200UL);

    // Idle timer starts at wateringTime - maxPumpTime = 800 ms; run it
    // partway down so there is a stale countdown in flight.
    g.tick(500UL);
    TEST_ASSERT_EQUAL_UINT32(300UL, g.getRemainingWateringInterval());

    // Simulate an incoming broker config update with a much longer interval.
    TEST_ASSERT_TRUE(g.setPumpTime(Channel::One, 200UL));
    TEST_ASSERT_TRUE(g.setPumpTime(Channel::Two, 200UL));
    TEST_ASSERT_TRUE(g.setWateringInterval(4000UL));

    // Without resetWateringTimer(), the countdown would still read the stale
    // 300 ms left over from the old interval.
    TEST_ASSERT_EQUAL_UINT32(300UL, g.getRemainingWateringInterval());

    TEST_ASSERT_TRUE(g.resetWateringTimer());

    // Now it reflects the new interval: 4000 - maxPumpTime(200) = 3800 ms.
    TEST_ASSERT_EQUAL_UINT32(3800UL, g.getRemainingWateringInterval());
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
    RUN_TEST(test_per_channel_pump_durations_are_independent);
    RUN_TEST(test_config_update_resets_watering_timer);
    return UNITY_END();
}
