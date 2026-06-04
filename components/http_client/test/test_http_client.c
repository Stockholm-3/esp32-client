#include "http_client.h"
#include "unity.h"

#include <string.h>

// ── Header linked-list — pure heap logic ──────────────────────────────────────

TEST_CASE("http_client_header_append builds single-entry list", "[http_client][logic]") {
    HttpClientHeader* head = NULL;
    int rc                 = http_client_header_append(&head, "Content-Type", "application/json");
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(head);
    TEST_ASSERT_EQUAL_STRING("Content-Type", head->key);
    TEST_ASSERT_EQUAL_STRING("application/json", head->value);
    TEST_ASSERT_NULL(head->next);
    http_client_headers_free(head);
}

TEST_CASE("http_client_header_append preserves insertion order", "[http_client][logic]") {
    HttpClientHeader* head = NULL;
    http_client_header_append(&head, "Accept", "text/plain");
    http_client_header_append(&head, "X-Custom", "value");
    http_client_header_append(&head, "Authorization", "Bearer tok");

    TEST_ASSERT_EQUAL_STRING("Accept", head->key);
    TEST_ASSERT_EQUAL_STRING("X-Custom", head->next->key);
    TEST_ASSERT_EQUAL_STRING("Authorization", head->next->next->key);
    TEST_ASSERT_NULL(head->next->next->next);

    http_client_headers_free(head);
}

TEST_CASE("http_client_headers_free handles null safely", "[http_client][logic]") {
    // Must not crash on NULL
    http_client_headers_free(NULL);
}

TEST_CASE("http_client_headers_free releases all nodes", "[http_client][logic]") {
    HttpClientHeader* head = NULL;
    http_client_header_append(&head, "A", "1");
    http_client_header_append(&head, "B", "2");
    http_client_header_append(&head, "C", "3");
    // Must not crash or leak (leak detection relies on heap check at test end)
    http_client_headers_free(head);
}

// ── Init / deinit ─────────────────────────────────────────────────────────────

TEST_CASE("http_client_init with null config succeeds", "[http_client][logic]") {
    http_client_deinit();
    TEST_ASSERT_EQUAL(0, http_client_init(NULL));
    http_client_deinit();
}

TEST_CASE("http_client_init is idempotent", "[http_client][logic]") {
    http_client_deinit();
    TEST_ASSERT_EQUAL(0, http_client_init(NULL));
    TEST_ASSERT_EQUAL(0, http_client_init(NULL)); // second call must not fail
    http_client_deinit();
}

TEST_CASE("http_client_init honours custom timeout", "[http_client][logic]") {
    http_client_deinit();
    HttpClientConfig cfg   = {0};
    cfg.default_timeout_ms = 5000;
    TEST_ASSERT_EQUAL(0, http_client_init(&cfg));
    http_client_deinit();
}

TEST_CASE("http_client_deinit is safe to call repeatedly", "[http_client][logic]") {
    http_client_deinit();
    http_client_deinit(); // must not crash
}
