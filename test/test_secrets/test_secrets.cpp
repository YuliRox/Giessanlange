#include <unity.h>
#include <map>
#include <string>

#include "Secrets.h"

namespace
{
// Fake key-value store backed by a std::map. Also counts puts so tests
// can assert idempotency.
struct FakeKv
{
    std::map<std::string, std::string> data;
    int putCount = 0;

    Secrets::KvStore make()
    {
        return Secrets::KvStore{
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

Secrets::BuildTimeValues makeBuildTime(
    const std::string &ssid = "",
    const std::string &pass = "",
    const std::string &user = "",
    const std::string &mpass = "",
    const std::string &broker = "")
{
    return Secrets::BuildTimeValues{ssid, pass, user, mpass, broker};
}
} // namespace

void test_first_boot_seeds_from_macros()
{
    FakeKv kv;
    Secrets secrets(kv.make(),
                    makeBuildTime("MyWifi", "MyPass", "u", "p", "192.168.1.10"));

    TEST_ASSERT_EQUAL_STRING("MyWifi", secrets.wifiSsid().c_str());
    TEST_ASSERT_EQUAL_STRING("MyPass", secrets.wifiPass().c_str());
    TEST_ASSERT_EQUAL_STRING("u", secrets.mqttUser().c_str());
    TEST_ASSERT_EQUAL_STRING("p", secrets.mqttPass().c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.10", secrets.mqttBroker().c_str());
    TEST_ASSERT_TRUE(secrets.hasCredentials());
    TEST_ASSERT_EQUAL_INT(5, kv.putCount);
}

void test_second_boot_is_idempotent_when_macros_unchanged()
{
    FakeKv kv;
    // First boot seeds.
    Secrets first(kv.make(), makeBuildTime("Net", "Pw", "u", "p", "broker"));
    const int afterFirst = kv.putCount;

    // Reset put counter; construct again with same macros. No new writes.
    kv.putCount = 0;
    Secrets second(kv.make(), makeBuildTime("Net", "Pw", "u", "p", "broker"));
    TEST_ASSERT_EQUAL_INT(0, kv.putCount);
    TEST_ASSERT_EQUAL_STRING("Net", second.wifiSsid().c_str());

    (void)afterFirst;
}

void test_changed_macros_reseed_nvs()
{
    FakeKv kv;
    Secrets first(kv.make(), makeBuildTime("OldNet", "OldPw", "", "", ""));
    TEST_ASSERT_EQUAL_STRING("OldNet", first.wifiSsid().c_str());

    kv.putCount = 0;
    Secrets second(kv.make(), makeBuildTime("NewNet", "NewPw", "", "", ""));
    TEST_ASSERT_EQUAL_STRING("NewNet", second.wifiSsid().c_str());
    TEST_ASSERT_EQUAL_STRING("NewPw", second.wifiPass().c_str());
    // Exactly two writes — the two changed keys.
    TEST_ASSERT_EQUAL_INT(2, kv.putCount);
}

void test_empty_macros_leave_nvs_intact()
{
    FakeKv kv;
    // Pre-populate the store as if a previous run had seeded it.
    kv.data["wifi_ssid"] = "Stored";
    kv.data["wifi_pass"] = "StoredPw";

    kv.putCount = 0;
    Secrets secrets(kv.make(), makeBuildTime("", "", "", "", ""));

    TEST_ASSERT_EQUAL_INT(0, kv.putCount);
    TEST_ASSERT_EQUAL_STRING("Stored", secrets.wifiSsid().c_str());
    TEST_ASSERT_EQUAL_STRING("StoredPw", secrets.wifiPass().c_str());
    TEST_ASSERT_TRUE(secrets.hasCredentials());
}

void test_empty_macros_and_empty_store_means_no_credentials()
{
    FakeKv kv;
    Secrets secrets(kv.make(), makeBuildTime("", "", "", "", ""));

    TEST_ASSERT_EQUAL_INT(0, kv.putCount);
    TEST_ASSERT_EQUAL_STRING("", secrets.wifiSsid().c_str());
    TEST_ASSERT_FALSE(secrets.hasCredentials());
}

void test_partial_macros_seed_only_the_provided_keys()
{
    FakeKv kv;
    kv.data["wifi_ssid"] = "AlreadyHere";
    kv.putCount = 0;

    // Only mqtt_broker is provided as a build-time value.
    Secrets secrets(kv.make(), makeBuildTime("", "", "", "", "mqtt.local"));

    // wifi_ssid stays as the pre-existing store value (no macro to reseed from).
    TEST_ASSERT_EQUAL_STRING("AlreadyHere", secrets.wifiSsid().c_str());
    // mqtt_broker was seeded from the macro.
    TEST_ASSERT_EQUAL_STRING("mqtt.local", secrets.mqttBroker().c_str());
    TEST_ASSERT_EQUAL_INT(1, kv.putCount);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_seeds_from_macros);
    RUN_TEST(test_second_boot_is_idempotent_when_macros_unchanged);
    RUN_TEST(test_changed_macros_reseed_nvs);
    RUN_TEST(test_empty_macros_leave_nvs_intact);
    RUN_TEST(test_empty_macros_and_empty_store_means_no_credentials);
    RUN_TEST(test_partial_macros_seed_only_the_provided_keys);
    return UNITY_END();
}
