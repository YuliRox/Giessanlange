#include <unity.h>
#include "DebouncedButton.h"

namespace
{
constexpr int OPEN = 1;
constexpr int CLOSED = 0;
constexpr unsigned long DEBOUNCE_MS = 50;
} // namespace

void test_fires_once_after_stable_low()
{
    int pin = OPEN;
    DebouncedButton btn([&] { return pin; }, DEBOUNCE_MS);

    pin = CLOSED;
    TEST_ASSERT_FALSE(btn.poll(0));
    TEST_ASSERT_FALSE(btn.poll(30));
    TEST_ASSERT_TRUE(btn.poll(60));
    TEST_ASSERT_TRUE(btn.isPressed());
}

void test_does_not_refire_while_held()
{
    int pin = OPEN;
    DebouncedButton btn([&] { return pin; }, DEBOUNCE_MS);

    pin = CLOSED;
    btn.poll(0);
    TEST_ASSERT_TRUE(btn.poll(60));
    TEST_ASSERT_FALSE(btn.poll(70));
    TEST_ASSERT_FALSE(btn.poll(500));
    TEST_ASSERT_FALSE(btn.poll(5000));
}

void test_bouncing_does_not_fire()
{
    int pin = OPEN;
    DebouncedButton btn([&] { return pin; }, DEBOUNCE_MS);

    // Rapid toggles all within the debounce window — never stable.
    pin = CLOSED;
    TEST_ASSERT_FALSE(btn.poll(0));
    pin = OPEN;
    TEST_ASSERT_FALSE(btn.poll(10));
    pin = CLOSED;
    TEST_ASSERT_FALSE(btn.poll(20));
    pin = OPEN;
    TEST_ASSERT_FALSE(btn.poll(40));
    // Settles open — no press should have fired.
    TEST_ASSERT_FALSE(btn.poll(120));
    TEST_ASSERT_FALSE(btn.isPressed());
}

void test_release_does_not_register_as_press()
{
    int pin = OPEN;
    DebouncedButton btn([&] { return pin; }, DEBOUNCE_MS);

    pin = CLOSED;
    btn.poll(0);
    TEST_ASSERT_TRUE(btn.poll(60));

    pin = OPEN;
    TEST_ASSERT_FALSE(btn.poll(100));
    TEST_ASSERT_FALSE(btn.poll(200));
    TEST_ASSERT_FALSE(btn.isPressed());
}

void test_is_pressed_tracks_confirmed_state()
{
    int pin = OPEN;
    DebouncedButton btn([&] { return pin; }, DEBOUNCE_MS);

    TEST_ASSERT_FALSE(btn.isPressed());

    pin = CLOSED;
    btn.poll(0);
    TEST_ASSERT_FALSE(btn.isPressed()); // not yet confirmed
    btn.poll(60);
    TEST_ASSERT_TRUE(btn.isPressed());

    pin = OPEN;
    btn.poll(100);
    TEST_ASSERT_TRUE(btn.isPressed()); // still in debounce window
    btn.poll(200);
    TEST_ASSERT_FALSE(btn.isPressed());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_fires_once_after_stable_low);
    RUN_TEST(test_does_not_refire_while_held);
    RUN_TEST(test_bouncing_does_not_fire);
    RUN_TEST(test_release_does_not_register_as_press);
    RUN_TEST(test_is_pressed_tracks_confirmed_state);
    return UNITY_END();
}
