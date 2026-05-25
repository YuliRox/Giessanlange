#include <unity.h>
#include <map>
#include <string>
#include <vector>

#include "MqttConfig.h"

namespace
{
struct KvFake
{
    std::map<std::string, std::string> data;
    int putCount = 0;

    MqttConfig::KvStore make()
    {
        return MqttConfig::KvStore{
            [this](const std::string &k) {
                auto it = data.find(k);
                return it == data.end() ? std::string() : it->second;
            },
            [this](const std::string &k, const std::string &v) {
                data[k] = v;
                ++putCount;
            },
        };
    }
};

struct PubFake
{
    struct Call { std::string topic, payload; bool retained; };
    std::vector<Call> calls;
    bool nextResult = true;

    MqttConfig::PublishFn fn()
    {
        return [this](const std::string &t, const std::string &p, bool r) {
            calls.push_back({t, p, r});
            return nextResult;
        };
    }
};

struct ApplyFake
{
    std::vector<MqttConfig::Values> applied;

    MqttConfig::ApplyFn fn()
    {
        return [this](const MqttConfig::Values &v) { applied.push_back(v); };
    }
};

MqttConfig::Config defaultConfig()
{
    MqttConfig::Config c;
    c.defaults.pumpTimeCh1Ms = 30000UL;
    c.defaults.pumpTimeCh2Ms = 30000UL;
    c.defaults.wateringIntervalMs = 86400000UL;
    return c;
}
} // namespace

void test_first_boot_no_nvs_seeds_defaults()
{
    KvFake kv;
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());

    auto v = cfg.initFromNvs();

    TEST_ASSERT_EQUAL_UINT32(30000UL, v.pumpTimeCh1Ms);
    TEST_ASSERT_EQUAL_UINT32(86400000UL, v.wateringIntervalMs);
    TEST_ASSERT_EQUAL_INT(3, kv.putCount);              // three keys seeded
    TEST_ASSERT_EQUAL_INT(1, (int)app.applied.size());  // applied to Giessanlage
}

void test_second_boot_with_nvs_no_writes()
{
    KvFake kv;
    kv.data["pump_time_ch1_ms"]     = "45000";
    kv.data["pump_time_ch2_ms"]     = "20000";
    kv.data["watering_interval_ms"] = "43200000";
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());

    auto v = cfg.initFromNvs();

    TEST_ASSERT_EQUAL_UINT32(45000UL, v.pumpTimeCh1Ms);
    TEST_ASSERT_EQUAL_UINT32(20000UL, v.pumpTimeCh2Ms);
    TEST_ASSERT_EQUAL_UINT32(43200000UL, v.wateringIntervalMs);
    TEST_ASSERT_EQUAL_INT(0, kv.putCount);
    TEST_ASSERT_EQUAL_INT(1, (int)app.applied.size());
}

void test_broker_config_matching_nvs_is_noop()
{
    KvFake kv;
    kv.data["pump_time_ch1_ms"]     = "30000";
    kv.data["pump_time_ch2_ms"]     = "30000";
    kv.data["watering_interval_ms"] = "86400000";
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());
    cfg.initFromNvs();
    kv.putCount = 0;
    app.applied.clear();

    cfg.onMqttConnected();
    const std::string payload =
        "{\"pump_time_ch1_ms\":30000,\"pump_time_ch2_ms\":30000,\"watering_interval_ms\":86400000}";
    bool changed = cfg.onConfigPayload(payload);

    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_INT(0, kv.putCount);
    TEST_ASSERT_EQUAL_INT(0, (int)app.applied.size());
    TEST_ASSERT_TRUE(cfg.brokerHasSpoken());
}

void test_broker_config_differing_overrides_nvs()
{
    KvFake kv;
    kv.data["pump_time_ch1_ms"]     = "30000";
    kv.data["pump_time_ch2_ms"]     = "30000";
    kv.data["watering_interval_ms"] = "86400000";
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());
    cfg.initFromNvs();
    kv.putCount = 0;
    app.applied.clear();

    cfg.onMqttConnected();
    const std::string payload =
        "{\"pump_time_ch1_ms\":45000,\"pump_time_ch2_ms\":15000,\"watering_interval_ms\":86400000}";
    bool changed = cfg.onConfigPayload(payload);

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_INT(3, kv.putCount); // all three rewritten
    TEST_ASSERT_EQUAL_INT(1, (int)app.applied.size());
    TEST_ASSERT_EQUAL_UINT32(45000UL, app.applied[0].pumpTimeCh1Ms);
    TEST_ASSERT_EQUAL_UINT32(15000UL, app.applied[0].pumpTimeCh2Ms);
}

void test_broker_silent_publishes_nvs_as_retained()
{
    KvFake kv;
    kv.data["pump_time_ch1_ms"]     = "30000";
    kv.data["pump_time_ch2_ms"]     = "30000";
    kv.data["watering_interval_ms"] = "86400000";
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());
    cfg.initFromNvs();
    cfg.onMqttConnected();
    // Grace expires without a config payload arriving.
    bool ok = cfg.publishIfBrokerSilent();

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());
    TEST_ASSERT_EQUAL_STRING("giessanlage/config", pub.calls[0].topic.c_str());
    TEST_ASSERT_TRUE(pub.calls[0].retained);
    TEST_ASSERT_TRUE(pub.calls[0].payload.find("\"pump_time_ch1_ms\":30000") != std::string::npos);

    // Second invocation no-ops because brokerHasSpoken is now true.
    TEST_ASSERT_FALSE(cfg.publishIfBrokerSilent());
    TEST_ASSERT_EQUAL_INT(1, (int)pub.calls.size());
}

void test_runtime_update_invalid_payload_rejected()
{
    KvFake kv;
    kv.data["pump_time_ch1_ms"]     = "30000";
    kv.data["pump_time_ch2_ms"]     = "30000";
    kv.data["watering_interval_ms"] = "86400000";
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());
    cfg.initFromNvs();
    kv.putCount = 0;
    app.applied.clear();
    cfg.onMqttConnected();

    // pump_time = 0 rejected
    TEST_ASSERT_FALSE(cfg.onConfigPayload(
        "{\"pump_time_ch1_ms\":0,\"pump_time_ch2_ms\":30000,\"watering_interval_ms\":86400000}"));
    // pump_time >= watering_interval rejected
    TEST_ASSERT_FALSE(cfg.onConfigPayload(
        "{\"pump_time_ch1_ms\":99999999,\"pump_time_ch2_ms\":30000,\"watering_interval_ms\":86400000}"));
    // malformed JSON rejected
    TEST_ASSERT_FALSE(cfg.onConfigPayload("{not json"));
    // missing field rejected
    TEST_ASSERT_FALSE(cfg.onConfigPayload(
        "{\"pump_time_ch1_ms\":30000,\"pump_time_ch2_ms\":30000}"));

    TEST_ASSERT_EQUAL_INT(0, kv.putCount);
    TEST_ASSERT_EQUAL_INT(0, (int)app.applied.size());
    TEST_ASSERT_FALSE(cfg.brokerHasSpoken()); // invalid payloads do not count
}

void test_reconnect_resets_broker_seen()
{
    KvFake kv;
    kv.data["pump_time_ch1_ms"]     = "30000";
    kv.data["pump_time_ch2_ms"]     = "30000";
    kv.data["watering_interval_ms"] = "86400000";
    PubFake pub;
    ApplyFake app;
    MqttConfig cfg(kv.make(), pub.fn(), app.fn(), defaultConfig());
    cfg.initFromNvs();

    cfg.onMqttConnected();
    cfg.onConfigPayload(
        "{\"pump_time_ch1_ms\":30000,\"pump_time_ch2_ms\":30000,\"watering_interval_ms\":86400000}");
    TEST_ASSERT_TRUE(cfg.brokerHasSpoken());

    // Disconnect + reconnect.
    cfg.onMqttConnected();
    TEST_ASSERT_FALSE(cfg.brokerHasSpoken());

    // Grace expires; should publish again.
    bool ok = cfg.publishIfBrokerSilent();
    TEST_ASSERT_TRUE(ok);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_no_nvs_seeds_defaults);
    RUN_TEST(test_second_boot_with_nvs_no_writes);
    RUN_TEST(test_broker_config_matching_nvs_is_noop);
    RUN_TEST(test_broker_config_differing_overrides_nvs);
    RUN_TEST(test_broker_silent_publishes_nvs_as_retained);
    RUN_TEST(test_runtime_update_invalid_payload_rejected);
    RUN_TEST(test_reconnect_resets_broker_seen);
    return UNITY_END();
}
