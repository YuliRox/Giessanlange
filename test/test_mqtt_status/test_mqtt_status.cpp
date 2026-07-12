#include <unity.h>
#include <string>
#include <vector>

#include "MqttStatus.h"

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

    MqttStatus::PublishFn fn()
    {
        return [this](const std::string &t, const std::string &p, bool r) {
            calls.push_back({t, p, r});
            return nextResult;
        };
    }
};

MqttStatus::Snapshot defaultSnapshot()
{
    MqttStatus::Snapshot s;
    s.stateCh1 = 1; // Idle
    s.stateCh2 = 1;
    s.remainingPumpMsCh1 = 0;
    s.remainingPumpMsCh2 = 0;
    s.remainingWateringMs = 23UL * 60UL * 60UL * 1000UL;
    s.pumpTimeCh1Ms = 30000;
    s.pumpTimeCh2Ms = 30000;
    s.wateringIntervalMs = 24UL * 60UL * 60UL * 1000UL;
    s.uptimeMs = 1000;
    return s;
}
} // namespace

void test_payload_schema()
{
    auto s = defaultSnapshot();
    s.stateCh1 = 2; // PumpingManual
    s.remainingPumpMsCh1 = 15000;
    s.uptimeMs = 42000;

    const std::string p = MqttStatus::buildPayload(s);

    TEST_ASSERT_TRUE(p.find("\"state_ch1\":\"PumpingManual\"") != std::string::npos);
    TEST_ASSERT_TRUE(p.find("\"state_ch2\":\"Idle\"") != std::string::npos);
    TEST_ASSERT_TRUE(p.find("\"remaining_pump_ms_ch1\":15000") != std::string::npos);
    TEST_ASSERT_TRUE(p.find("\"watering_interval_ms\":86400000") != std::string::npos);
    TEST_ASSERT_TRUE(p.find("\"uptime_ms\":42000") != std::string::npos);
}

void test_first_call_publishes()
{
    PublishFake pub;
    MqttStatus status(pub.fn(), {});
    auto s = defaultSnapshot();

    TEST_ASSERT_TRUE(status.update(s, 0));
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());
    TEST_ASSERT_EQUAL_STRING("giessanlage/status", pub.calls[0].topic.c_str());
    TEST_ASSERT_TRUE(pub.calls[0].retained);
}

void test_unchanged_snapshot_no_publish()
{
    PublishFake pub;
    MqttStatus status(pub.fn(), {});
    auto s = defaultSnapshot();

    status.update(s, 0);
    // Same snapshot, past the change-throttle but before the heartbeat: no publish.
    TEST_ASSERT_FALSE(status.update(s, 2000));
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());
}

void test_heartbeat_republishes_unchanged_snapshot()
{
    PublishFake pub;
    MqttStatus::Config cfg;
    cfg.heartbeatIntervalMs = 30000;
    MqttStatus status(pub.fn(), cfg);
    auto s = defaultSnapshot();

    status.update(s, 0);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // Only uptime advances (not a meaningful field). Before the heartbeat
    // interval: still no republish.
    s.uptimeMs = 20000;
    TEST_ASSERT_FALSE(status.update(s, 20000));
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // Once the heartbeat interval elapses, republish even though only uptime changed.
    s.uptimeMs = 30000;
    TEST_ASSERT_TRUE(status.update(s, 30000));
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
}

void test_throttle_holds_back_rapid_changes()
{
    PublishFake pub;
    MqttStatus::Config cfg;
    cfg.minIntervalMs = 1000;
    MqttStatus status(pub.fn(), cfg);

    auto s = defaultSnapshot();
    status.update(s, 0);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // Change a meaningful field rapidly — three updates within the throttle
    // window. (uptimeMs is not meaningful, so use a real state field.)
    s.remainingWateringMs = 100;
    status.update(s, 100);
    s.remainingWateringMs = 500;
    status.update(s, 500);
    s.remainingWateringMs = 900;
    status.update(s, 900);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // After the window elapses the next change publishes.
    s.remainingWateringMs = 1100;
    status.update(s, 1100);
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
}

void test_idle_change_uses_wider_throttle()
{
    PublishFake pub;
    MqttStatus::Config cfg;
    cfg.minIntervalMs = 1000;
    cfg.idleIntervalMs = 10000;
    MqttStatus status(pub.fn(), cfg);

    auto s = defaultSnapshot();
    s.allIdle = true;
    status.update(s, 0);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // remainingWateringMs keeps ticking down while idle; past the (active)
    // minIntervalMs but still within idleIntervalMs — should not republish.
    s.remainingWateringMs = 900;
    TEST_ASSERT_FALSE(status.update(s, 1500));
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // Past idleIntervalMs: republishes.
    s.remainingWateringMs = 500;
    TEST_ASSERT_TRUE(status.update(s, 10000));
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
}

void test_publish_failure_does_not_record_as_published()
{
    PublishFake pub;
    MqttStatus status(pub.fn(), {});
    auto s = defaultSnapshot();

    pub.nextResult = false;
    TEST_ASSERT_FALSE(status.update(s, 0));
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size()); // attempted but failed

    // Restore success; same snapshot must still publish (not coalesced)
    // because the failure means we never recorded it as published.
    pub.nextResult = true;
    TEST_ASSERT_TRUE(status.update(s, 1500));
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
}

void test_invalidate_forces_republish_even_if_snapshot_unchanged()
{
    PublishFake pub;
    MqttStatus status(pub.fn(), {});
    auto s = defaultSnapshot();

    status.update(s, 0);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // No change, well past throttle — would normally no-op.
    TEST_ASSERT_FALSE(status.update(s, 5000));
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());

    // After invalidate(), next update republishes.
    status.invalidate();
    TEST_ASSERT_TRUE(status.update(s, 6000));
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
}

void test_changed_snapshot_after_throttle_publishes()
{
    PublishFake pub;
    MqttStatus status(pub.fn(), {});
    auto s = defaultSnapshot();

    status.update(s, 0);

    s.stateCh1 = 2; // Idle -> PumpingManual
    s.remainingPumpMsCh1 = 30000;
    TEST_ASSERT_TRUE(status.update(s, 2000));
    TEST_ASSERT_EQUAL_INT(2, (int)pub.calls.size());
    TEST_ASSERT_TRUE(pub.calls[1].payload.find("PumpingManual") != std::string::npos);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_payload_schema);
    RUN_TEST(test_first_call_publishes);
    RUN_TEST(test_unchanged_snapshot_no_publish);
    RUN_TEST(test_heartbeat_republishes_unchanged_snapshot);
    RUN_TEST(test_throttle_holds_back_rapid_changes);
    RUN_TEST(test_idle_change_uses_wider_throttle);
    RUN_TEST(test_publish_failure_does_not_record_as_published);
    RUN_TEST(test_invalidate_forces_republish_even_if_snapshot_unchanged);
    RUN_TEST(test_changed_snapshot_after_throttle_publishes);
    return UNITY_END();
}
