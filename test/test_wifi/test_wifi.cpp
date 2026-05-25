#include <unity.h>
#include <string>

#include "WifiManager.h"

namespace
{
struct WifiFake
{
    int beginCalls = 0;
    std::string lastSsid;
    std::string lastPass;
    bool connected = false;

    WifiManager::BeginConnectFn beginFn()
    {
        return [this](const std::string &ssid, const std::string &pass) {
            ++beginCalls;
            lastSsid = ssid;
            lastPass = pass;
        };
    }

    WifiManager::IsConnectedFn isConnectedFn()
    {
        return [this]() { return connected; };
    }
};

WifiManager::Config testConfig()
{
    WifiManager::Config c;
    c.initialBackoffMs   = 100;
    c.maxBackoffMs       = 800;
    c.connectTimeoutMs   = 500;
    return c;
}
} // namespace

void test_no_credentials_stays_disconnected_and_never_calls_begin()
{
    WifiFake f;
    WifiManager mgr(f.beginFn(), f.isConnectedFn(), "", "", testConfig());

    for (int i = 0; i < 100; ++i)
        mgr.tick(50);

    TEST_ASSERT_EQUAL(WifiManager::State::Disconnected, mgr.state());
    TEST_ASSERT_FALSE(mgr.isConnected());
    TEST_ASSERT_EQUAL_INT(0, f.beginCalls);
}

void test_happy_path_disconnected_to_connected()
{
    WifiFake f;
    WifiManager mgr(f.beginFn(), f.isConnectedFn(), "Net", "Pw", testConfig());

    // First tick after initial backoff window kicks the begin call.
    bool changed = mgr.tick(100);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(WifiManager::State::Connecting, mgr.state());
    TEST_ASSERT_EQUAL_INT(1, f.beginCalls);
    TEST_ASSERT_EQUAL_STRING("Net", f.lastSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("Pw", f.lastPass.c_str());

    // AP associates partway through the window.
    f.connected = true;
    changed = mgr.tick(50);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(WifiManager::State::Connected, mgr.state());
    TEST_ASSERT_TRUE(mgr.isConnected());
    TEST_ASSERT_EQUAL_UINT(0, mgr.failedAttempts());
}

void test_connect_timeout_increments_backoff()
{
    WifiFake f;
    WifiManager mgr(f.beginFn(), f.isConnectedFn(), "Net", "Pw", testConfig());

    // Kick first attempt.
    mgr.tick(100);
    TEST_ASSERT_EQUAL(WifiManager::State::Connecting, mgr.state());
    TEST_ASSERT_EQUAL_INT(1, f.beginCalls);

    // AP never associates; ride past the connectTimeoutMs (500 ms).
    mgr.tick(600);
    TEST_ASSERT_EQUAL(WifiManager::State::Disconnected, mgr.state());
    TEST_ASSERT_EQUAL_UINT(1, mgr.failedAttempts());

    // Backoff is now 200 ms. Tick 199 ms — still no new begin.
    mgr.tick(199);
    TEST_ASSERT_EQUAL_INT(1, f.beginCalls);

    // One more ms; backoff elapses; second attempt fires.
    mgr.tick(1);
    TEST_ASSERT_EQUAL(WifiManager::State::Connecting, mgr.state());
    TEST_ASSERT_EQUAL_INT(2, f.beginCalls);
}

void test_backoff_caps_at_max()
{
    WifiFake f;
    WifiManager mgr(f.beginFn(), f.isConnectedFn(), "Net", "Pw", testConfig());

    // Force five failed attempts; the backoff would double 100→200→400→800
    // (cap)→800.
    for (int i = 0; i < 5; ++i)
    {
        // Wait long enough to start an attempt, then ride past the timeout.
        mgr.tick(2000);  // generous: passes both backoff and timeout
        mgr.tick(2000);
    }

    TEST_ASSERT_EQUAL(WifiManager::State::Disconnected, mgr.state());
    TEST_ASSERT_TRUE(mgr.failedAttempts() >= 3);

    // Next attempt should wait at most maxBackoffMs.
    // Tick just under 800; no new begin yet.
    const int beginsBefore = f.beginCalls;
    mgr.tick(799);
    TEST_ASSERT_EQUAL_INT(beginsBefore, f.beginCalls);
    mgr.tick(2);
    TEST_ASSERT_EQUAL_INT(beginsBefore + 1, f.beginCalls);
}

void test_dropped_connection_returns_to_disconnected_and_resets_backoff()
{
    WifiFake f;
    WifiManager mgr(f.beginFn(), f.isConnectedFn(), "Net", "Pw", testConfig());

    // Reach Connected.
    mgr.tick(100);
    f.connected = true;
    mgr.tick(50);
    TEST_ASSERT_EQUAL(WifiManager::State::Connected, mgr.state());

    // AP drops.
    f.connected = false;
    bool changed = mgr.tick(10);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL(WifiManager::State::Disconnected, mgr.state());

    // Backoff should be back to the initial value, not whatever it was
    // before the successful association.
    mgr.tick(100);
    TEST_ASSERT_EQUAL(WifiManager::State::Connecting, mgr.state());
    TEST_ASSERT_EQUAL_INT(2, f.beginCalls);
}

void test_tick_never_calls_begin_twice_in_one_attempt()
{
    WifiFake f;
    WifiManager mgr(f.beginFn(), f.isConnectedFn(), "Net", "Pw", testConfig());

    // Kick first attempt.
    mgr.tick(100);
    TEST_ASSERT_EQUAL_INT(1, f.beginCalls);

    // Many idle ticks while waiting for association — begin must NOT
    // refire even if elapsedInState keeps growing.
    for (int i = 0; i < 4; ++i)
        mgr.tick(50);  // total 200 ms, still under connectTimeoutMs

    TEST_ASSERT_EQUAL_INT(1, f.beginCalls);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_credentials_stays_disconnected_and_never_calls_begin);
    RUN_TEST(test_happy_path_disconnected_to_connected);
    RUN_TEST(test_connect_timeout_increments_backoff);
    RUN_TEST(test_backoff_caps_at_max);
    RUN_TEST(test_dropped_connection_returns_to_disconnected_and_resets_backoff);
    RUN_TEST(test_tick_never_calls_begin_twice_in_one_attempt);
    return UNITY_END();
}
