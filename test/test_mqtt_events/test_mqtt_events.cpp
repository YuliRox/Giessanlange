#include <unity.h>
#include <string>
#include <vector>

#include "MqttEvents.h"

namespace
{
struct PublishFake
{
    struct Call
    {
        std::string topic;
        std::string payload;
        bool retained;
    };
    std::vector<Call> calls;
    bool nextResult = true;

    MqttEvents::PublishFn fn()
    {
        return [this](const std::string &t, const std::string &p, bool r) {
            calls.push_back({t, p, r});
            return nextResult;
        };
    }
};

constexpr int IDLE = 1;
constexpr int PUMPING_MANUAL = 2;
constexpr int PUMPING_AUTO = 3;
} // namespace

void test_first_call_baselines_silently()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});

    int n = ev.updatePumpStates({IDLE, IDLE}, 100);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_EQUAL_INT(0, (int)pub.calls.size());
}

void test_single_channel_transition_emits_one_event()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});
    ev.updatePumpStates({IDLE, IDLE}, 0);

    int n = ev.updatePumpStates({PUMPING_MANUAL, IDLE}, 100);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    const auto &c = pub.calls[0];
    TEST_ASSERT_EQUAL_STRING("giessanlage/events/pump", c.topic.c_str());
    TEST_ASSERT_FALSE(c.retained);
    TEST_ASSERT_TRUE(c.payload.find("\"channel\":1") != std::string::npos);
    TEST_ASSERT_TRUE(c.payload.find("\"from\":\"Idle\"") != std::string::npos);
    TEST_ASSERT_TRUE(c.payload.find("\"to\":\"PumpingManual\"") != std::string::npos);
    TEST_ASSERT_TRUE(c.payload.find("\"ts_ms\":100") != std::string::npos);
}

void test_simultaneous_two_channel_transitions_emit_two_events()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});
    ev.updatePumpStates({IDLE, IDLE}, 0);

    int n = ev.updatePumpStates({PUMPING_AUTO, PUMPING_AUTO}, 200);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
    TEST_ASSERT_TRUE(pub.calls[0].payload.find("\"channel\":1") != std::string::npos);
    TEST_ASSERT_TRUE(pub.calls[1].payload.find("\"channel\":2") != std::string::npos);
}

void test_no_change_no_event()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});
    ev.updatePumpStates({PUMPING_MANUAL, IDLE}, 0);

    int n = ev.updatePumpStates({PUMPING_MANUAL, IDLE}, 100);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_EQUAL_INT(0, (int)pub.calls.size());
}

void test_button_event_payload()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});

    bool ok = ev.publishButton("pump1", 5000);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    const auto &c = pub.calls[0];
    TEST_ASSERT_EQUAL_STRING("giessanlage/events/button", c.topic.c_str());
    TEST_ASSERT_FALSE(c.retained);
    TEST_ASSERT_TRUE(c.payload.find("\"button\":\"pump1\"") != std::string::npos);
    TEST_ASSERT_TRUE(c.payload.find("\"ts_ms\":5000") != std::string::npos);
}

void test_offline_drops_events_silently_and_keeps_advancing_state()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});
    ev.updatePumpStates({IDLE, IDLE}, 0);

    pub.nextResult = false;
    int n = ev.updatePumpStates({PUMPING_AUTO, IDLE}, 100);
    TEST_ASSERT_EQUAL_INT(0, n);                         // failed publish
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());     // was attempted

    // Crucially: state still advanced. A subsequent same-state call
    // doesn't re-emit the transition (no infinite retry).
    pub.nextResult = true;
    n = ev.updatePumpStates({PUMPING_AUTO, IDLE}, 200);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // Next genuine transition fires normally.
    n = ev.updatePumpStates({IDLE, IDLE}, 300);
    TEST_ASSERT_EQUAL_INT(1, n);
}

void test_reset_baseline_silences_next_call()
{
    PublishFake pub;
    MqttEvents ev(pub.fn(), {});
    ev.updatePumpStates({IDLE, IDLE}, 0);
    ev.updatePumpStates({PUMPING_MANUAL, IDLE}, 100);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // Simulate MQTT disconnect/reconnect.
    ev.resetBaseline();

    // Next call must baseline silently even though the state differs
    // from what was last seen pre-disconnect — we'd otherwise spam
    // post-reconnect transitions that already happened offline.
    int n = ev.updatePumpStates({PUMPING_AUTO, PUMPING_AUTO}, 200);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size()); // unchanged

    // Real transition after the re-baseline emits.
    n = ev.updatePumpStates({IDLE, PUMPING_AUTO}, 300);
    TEST_ASSERT_EQUAL_INT(1, n);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_call_baselines_silently);
    RUN_TEST(test_single_channel_transition_emits_one_event);
    RUN_TEST(test_simultaneous_two_channel_transitions_emit_two_events);
    RUN_TEST(test_no_change_no_event);
    RUN_TEST(test_button_event_payload);
    RUN_TEST(test_offline_drops_events_silently_and_keeps_advancing_state);
    RUN_TEST(test_reset_baseline_silences_next_call);
    return UNITY_END();
}
