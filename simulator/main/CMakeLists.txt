idf_component_register(
    SRCS "../../main/main.c"
    INCLUDE_DIRS "../../main"
    REQUIRES display ui log time_manager ui_binder settings_manager
)
