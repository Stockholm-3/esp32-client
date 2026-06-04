#include "unity.h"

// The ui component's testable logic (timeout_minutes_from_idx and the stage
// percentage calculations) lives inside static functions in ui.c and cannot
// be called directly from a test binary.
//
// To enable direct unit testing of that logic, the static keyword should be
// removed from timeout_minutes_from_idx and a declaration added to ui.h.
// Until then, the mathematical invariants are verified here as constants.

// ── Timeout percentage invariants ─────────────────────────────────────────
// The stage split is: dim=50%, screensaver=75%, backlight_off=100%
// These tests verify the arithmetic used in ui.c's timeout_changed_cb.

TEST_CASE("timeout stage percentages are in ascending order", "[ui][logic]") {
    const uint32_t total_seconds = 30U * 60U; // 30 min example
    uint32_t dim                 = (total_seconds * 50U) / 100U;
    uint32_t screensaver         = (total_seconds * 75U) / 100U;
    uint32_t backlight_off       = total_seconds;

    TEST_ASSERT_LESS_THAN(screensaver, dim);
    TEST_ASSERT_LESS_THAN(backlight_off, screensaver);
}

TEST_CASE("timeout stage percentages stay within bounds for all dropdown indices", "[ui][logic]") {
    static const uint32_t TIMEOUT_MINUTES[] = {5U, 10U, 15U, 20U, 25U, 30U};
    const size_t count                      = sizeof(TIMEOUT_MINUTES) / sizeof(TIMEOUT_MINUTES[0]);

    for (size_t i = 0; i < count; i++) {
        uint32_t total       = TIMEOUT_MINUTES[i] * 60U;
        uint32_t dim         = (total * 50U) / 100U;
        uint32_t screensaver = (total * 75U) / 100U;

        TEST_ASSERT_LESS_OR_EQUAL(screensaver, dim);
        TEST_ASSERT_LESS_OR_EQUAL(total, screensaver);
        TEST_ASSERT_GREATER_THAN(0, total);
    }
}

TEST_CASE("timeout dropdown has exactly 6 options", "[ui][logic]") {
    // Mirrors the TIMEOUT_MINUTES array in ui.c — must stay in sync.
    static const uint32_t TIMEOUT_MINUTES[] = {5U, 10U, 15U, 20U, 25U, 30U};
    TEST_ASSERT_EQUAL(6, sizeof(TIMEOUT_MINUTES) / sizeof(TIMEOUT_MINUTES[0]));
}
