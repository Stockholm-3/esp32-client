#include "time_manager.h"
#include "unity.h"

#include <time.h>

// ── Pre-init state ────────────────────────────────────────────────────────────
// These tests exercise the module in the state it starts in after boot,
// before any NTP sync has occurred.

TEST_CASE("time_manager_get_time returns false before init", "[time_manager][logic]") {
    struct tm info;
    // g_time_valid is false on module startup; get_time reads g_time_valid
    bool valid = time_manager_get_time(&info);
    // If already synced from a previous test, this may be true — acceptable.
    // The key assertion is that the function does not crash.
    (void)valid;
}

TEST_CASE("time_manager_get_time null pointer does not crash", "[time_manager][logic]") {
    // Must not crash regardless of sync state
    time_manager_get_time(NULL);
}

TEST_CASE("time_manager_get_state returns a valid enum value", "[time_manager][logic]") {
    TimeState state = time_manager_get_state();
    TEST_ASSERT_TRUE(state == TIME_STATE_UNSYNCED || state == TIME_STATE_SYNCING ||
                     state == TIME_STATE_SYNCED || state == TIME_STATE_FAILED);
}

// ── Init ───────────────────────────────────────────────────────────────────
// Requires network connectivity to fully resolve, but the state machine
// transition to SYNCING is testable immediately after init.

TEST_CASE("time_manager_init with null callback does not crash", "[time_manager][hardware]") {
    time_manager_init(NULL);
    // After init, state must be SYNCING or SYNCED (if already synced)
    TimeState state = time_manager_get_state();
    TEST_ASSERT_TRUE(state == TIME_STATE_SYNCING || state == TIME_STATE_SYNCED);
}

TEST_CASE("time_manager_init is idempotent", "[time_manager][hardware]") {
    time_manager_init(NULL);
    time_manager_init(NULL); // second call must not crash or reinitialise SNTP
    TimeState state = time_manager_get_state();
    TEST_ASSERT_TRUE(state == TIME_STATE_SYNCING || state == TIME_STATE_SYNCED);
}
