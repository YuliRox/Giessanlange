#include <unity.h>

#include "Giessanlage.h"
#include "MqttCommands.h"

using Channel = Giessanlage::Channel;

void test_toggle_pump_starts_idle_channel()
{
    Giessanlage g;
    MqttCommands cmds;

    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"toggle_pump\"}", 1000UL));
    TEST_ASSERT_TRUE(g.isPumping(Channel::One));
}

void test_toggle_pump_stops_pumping_channel()
{
    Giessanlage g;
    MqttCommands cmds;

    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"toggle_pump\"}", 1000UL));
    TEST_ASSERT_FALSE(g.isPumping(Channel::One));
}

void test_set_timer_updates_pump_time_without_restarting_countdown()
{
    Giessanlage g(10000UL, 200UL, 200UL);
    MqttCommands cmds;

    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    g.tick(150UL);
    TEST_ASSERT_EQUAL_UINT32(50UL, g.getRemainingPumpTime(Channel::One));

    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One,
        "{\"cmd\":\"set_timer\",\"time_ms\":500}", 1000UL));

    TEST_ASSERT_EQUAL_UINT32(500UL, g.getPumpTime(Channel::One));
    // Countdown must not be restarted by set_timer.
    TEST_ASSERT_EQUAL_UINT32(50UL, g.getRemainingPumpTime(Channel::One));
}

void test_reset_timer_updates_pump_time_and_restarts_countdown()
{
    Giessanlage g(10000UL, 200UL, 200UL);
    MqttCommands cmds;

    TEST_ASSERT_TRUE(g.triggerPump(Channel::One));
    g.tick(150UL);
    TEST_ASSERT_EQUAL_UINT32(50UL, g.getRemainingPumpTime(Channel::One));

    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One,
        "{\"cmd\":\"reset_timer\",\"time_ms\":500}", 1000UL));

    TEST_ASSERT_EQUAL_UINT32(500UL, g.getPumpTime(Channel::One));
    TEST_ASSERT_EQUAL_UINT32(500UL, g.getRemainingPumpTime(Channel::One));
}

void test_malformed_and_unknown_commands_are_rejected()
{
    Giessanlage g(10000UL, 200UL, 200UL);
    MqttCommands cmds;

    TEST_ASSERT_FALSE(cmds.dispatch(g, Channel::One, "not json", 1000UL));
    TEST_ASSERT_FALSE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"explode\"}", 1000UL));
    TEST_ASSERT_FALSE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"set_timer\"}", 1000UL));
    TEST_ASSERT_FALSE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"reset_timer\"}", 1000UL));

    TEST_ASSERT_FALSE(g.isPumping(Channel::One));
    TEST_ASSERT_EQUAL_UINT32(200UL, g.getPumpTime(Channel::One));
}

void test_rate_limits_same_channel_within_window()
{
    Giessanlage g;
    MqttCommands cmds;

    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"toggle_pump\"}", 1000UL));
    TEST_ASSERT_TRUE(g.isPumping(Channel::One));

    // Second command <200ms later on the same channel is rate-limited: no
    // Giessanlage call happens, so the pump stays on rather than toggling off.
    TEST_ASSERT_FALSE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"toggle_pump\"}", 1100UL));
    TEST_ASSERT_TRUE(g.isPumping(Channel::One));

    // Past the window, the command goes through again.
    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"toggle_pump\"}", 1201UL));
    TEST_ASSERT_FALSE(g.isPumping(Channel::One));
}

void test_rate_limit_slots_are_independent_per_channel()
{
    Giessanlage g;
    MqttCommands cmds;

    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::One, "{\"cmd\":\"toggle_pump\"}", 1000UL));
    // Ch2 command arrives 50ms later — must not be blocked by ch1's slot.
    TEST_ASSERT_TRUE(cmds.dispatch(g, Channel::Two, "{\"cmd\":\"toggle_pump\"}", 1050UL));

    TEST_ASSERT_TRUE(g.isPumping(Channel::One));
    TEST_ASSERT_TRUE(g.isPumping(Channel::Two));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_toggle_pump_starts_idle_channel);
    RUN_TEST(test_toggle_pump_stops_pumping_channel);
    RUN_TEST(test_set_timer_updates_pump_time_without_restarting_countdown);
    RUN_TEST(test_reset_timer_updates_pump_time_and_restarts_countdown);
    RUN_TEST(test_malformed_and_unknown_commands_are_rejected);
    RUN_TEST(test_rate_limits_same_channel_within_window);
    RUN_TEST(test_rate_limit_slots_are_independent_per_channel);
    return UNITY_END();
}
