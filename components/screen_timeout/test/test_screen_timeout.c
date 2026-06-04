#include "screen_timeout.h"
#include "unity.h"

// screen_timeout_set_config calls screen_timeout_record_activity() internally,
// which calls lv_tick_get(). On ESP32-S3, lv_tick_get() reads esp_timer_get_time()
// which is always available. These tests are therefore safe without display hardware,
// but lv_init() should be called before running this suite if linking with LVGL.

// ── Round-trip: set_config / get_config ────────────────────────────────────

TEST_CASE("screen_timeout set/get round-trips valid config", "[screen_timeout][logic]") {
    ScreenTimeoutConfig in = {
        .dim_timeout_seconds           = 300,
        .screensaver_timeout_seconds   = 600,
        .backlight_off_timeout_seconds = 1800,
    };
    screen_timeout_set_config(&in);

    ScreenTimeoutConfig out = {0};
    screen_timeout_get_config(&out);

    TEST_ASSERT_EQUAL(300, out.dim_timeout_seconds);
    TEST_ASSERT_EQUAL(600, out.screensaver_timeout_seconds);
    TEST_ASSERT_EQUAL(1800, out.backlight_off_timeout_seconds);
}

// ── Config normalization: inverted stages ────────────────────────────────

TEST_CASE("screen_timeout normalizes screensaver < dim to same value", "[screen_timeout][logic]") {
    // screensaver (250s) < dim (300s) → dim must be clamped to 250
    ScreenTimeoutConfig in = {
        .dim_timeout_seconds           = 300,
        .screensaver_timeout_seconds   = 250,
        .backlight_off_timeout_seconds = 1800,
    };
    screen_timeout_set_config(&in);

    ScreenTimeoutConfig out = {0};
    screen_timeout_get_config(&out);

    TEST_ASSERT_TRUE(out.dim_timeout_seconds <= out.screensaver_timeout_seconds);
}

TEST_CASE("screen_timeout normalizes backlight_off < screensaver", "[screen_timeout][logic]") {
    // backlight_off (500s) < screensaver (600s) → screensaver must be clamped
    ScreenTimeoutConfig in = {
        .dim_timeout_seconds           = 300,
        .screensaver_timeout_seconds   = 600,
        .backlight_off_timeout_seconds = 500,
    };
    screen_timeout_set_config(&in);

    ScreenTimeoutConfig out = {0};
    screen_timeout_get_config(&out);

    TEST_ASSERT_TRUE(out.screensaver_timeout_seconds <= out.backlight_off_timeout_seconds);
    TEST_ASSERT_TRUE(out.dim_timeout_seconds <= out.screensaver_timeout_seconds);
}

TEST_CASE("screen_timeout stages are always in non-decreasing order after set",
          "[screen_timeout][logic]") {
    ScreenTimeoutConfig in = {
        .dim_timeout_seconds           = 1000,
        .screensaver_timeout_seconds   = 500,
        .backlight_off_timeout_seconds = 200,
    };
    screen_timeout_set_config(&in);

    ScreenTimeoutConfig out = {0};
    screen_timeout_get_config(&out);

    TEST_ASSERT_TRUE(out.dim_timeout_seconds <= out.screensaver_timeout_seconds);
    TEST_ASSERT_TRUE(out.screensaver_timeout_seconds <= out.backlight_off_timeout_seconds);
}

// ── Null and zero handling ─────────────────────────────────────────────────

TEST_CASE("screen_timeout set_config null pointer is safe", "[screen_timeout][logic]") {
    // Must not crash
    screen_timeout_set_config(NULL);
}

TEST_CASE("screen_timeout get_config null pointer is safe", "[screen_timeout][logic]") {
    // Must not crash
    screen_timeout_get_config(NULL);
}

TEST_CASE("screen_timeout disabled stages (zero) pass through unchanged",
          "[screen_timeout][logic]") {
    ScreenTimeoutConfig in = {
        .dim_timeout_seconds           = 0,
        .screensaver_timeout_seconds   = 0,
        .backlight_off_timeout_seconds = 900,
    };
    screen_timeout_set_config(&in);

    ScreenTimeoutConfig out = {0};
    screen_timeout_get_config(&out);

    TEST_ASSERT_EQUAL(0, out.dim_timeout_seconds);
    TEST_ASSERT_EQUAL(0, out.screensaver_timeout_seconds);
    TEST_ASSERT_EQUAL(900, out.backlight_off_timeout_seconds);
}
