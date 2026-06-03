#include "unity.h"

void app_main(void) {
    UNITY_BEGIN();

    // This looks up every TEST_CASE defined in your components and runs them sequentially
    unity_run_all_tests();

    UNITY_END();
}
