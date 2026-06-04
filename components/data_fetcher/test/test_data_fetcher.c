#include "data_fetcher.h"
#include "unity.h"

// ── Default config — verifies all macro defaults are wired correctly ──────────

TEST_CASE("data_fetcher_default_config sets correct elpris trigger hour", "[data_fetcher][logic]") {
    DataFetcherConfig cfg = data_fetcher_default_config();
    TEST_ASSERT_EQUAL(DATA_FETCHER_ELPRIS_HOUR_DEFAULT, cfg.elpris_trigger_hour);
    TEST_ASSERT_TRUE(cfg.elpris_trigger_hour <= 23);
}

TEST_CASE("data_fetcher_default_config sets correct weather interval", "[data_fetcher][logic]") {
    DataFetcherConfig cfg = data_fetcher_default_config();
    TEST_ASSERT_EQUAL(DATA_FETCHER_WEATHER_INTERVAL_SEC_DEFAULT, cfg.weather_interval_sec);
}

TEST_CASE("data_fetcher_default_config sets correct retry limits", "[data_fetcher][logic]") {
    DataFetcherConfig cfg = data_fetcher_default_config();
    TEST_ASSERT_EQUAL(DATA_FETCHER_MAX_RETRIES_DEFAULT, cfg.max_retries);
    TEST_ASSERT_EQUAL(DATA_FETCHER_RETRY_BASE_MS_DEFAULT, cfg.retry_base_ms);
    TEST_ASSERT_EQUAL(DATA_FETCHER_RETRY_MAX_MS_DEFAULT, cfg.retry_max_ms);
}

TEST_CASE("data_fetcher_default_config retry_max_ms exceeds retry_base_ms",
          "[data_fetcher][logic]") {
    DataFetcherConfig cfg = data_fetcher_default_config();
    TEST_ASSERT_GREATER_THAN(cfg.retry_base_ms, cfg.retry_max_ms);
}

TEST_CASE("data_fetcher_default_config leaves URLs as NULL", "[data_fetcher][logic]") {
    DataFetcherConfig cfg = data_fetcher_default_config();
    TEST_ASSERT_NULL(cfg.elpris_url);
    TEST_ASSERT_NULL(cfg.weather_url);
}

TEST_CASE("data_fetcher_default_config callback is NULL", "[data_fetcher][logic]") {
    DataFetcherConfig cfg = data_fetcher_default_config();
    TEST_ASSERT_NULL(cfg.on_cached);
    TEST_ASSERT_NULL(cfg.on_cached_ctx);
}

// ── Init argument validation ──────────────────────────────────────────────────

TEST_CASE("data_fetcher_init rejects null config", "[data_fetcher][logic]") {
    TEST_ASSERT_EQUAL(-1, data_fetcher_init(NULL, NULL));
}

// ── Cache key constants ───────────────────────────────────────────────────────

TEST_CASE("cache key for elpris is correct", "[data_fetcher][logic]") {
    TEST_ASSERT_EQUAL_STRING("data_fetcher:elpris", DATA_FETCHER_CACHE_KEY_ELPRIS);
}

TEST_CASE("cache key for weather is correct", "[data_fetcher][logic]") {
    TEST_ASSERT_EQUAL_STRING("data_fetcher:weather", DATA_FETCHER_CACHE_KEY_WEATHER);
}

TEST_CASE("cache keys are distinct", "[data_fetcher][logic]") {
    TEST_ASSERT_NOT_EQUAL(0, strcmp(DATA_FETCHER_CACHE_KEY_ELPRIS, DATA_FETCHER_CACHE_KEY_WEATHER));
}
