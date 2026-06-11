idf_component_register(
    SRCS "data_fetcher.c"
    INCLUDE_DIRS "include"
    REQUIRES
    lib
    http_client
    cache_fs
    wifi_manager
    log
)
