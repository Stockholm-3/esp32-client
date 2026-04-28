#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

// Background layers (dark navy-gray)
#define UI_COLOR_BG0    lv_color_hex(0x1E1E2E)  // deepest bg, screen root
#define UI_COLOR_BG1    lv_color_hex(0x252535)  // card bg
#define UI_COLOR_BG2    lv_color_hex(0x2E2E40)  // input / inset bg
#define UI_COLOR_BG3    lv_color_hex(0x383850)  // raised element bg

// Borders
#define UI_COLOR_LINE       lv_color_hex(0x404058)
#define UI_COLOR_LINE_SOFT  lv_color_hex(0x343448)

// Text (ink)
#define UI_COLOR_INK1   lv_color_hex(0xF3F3F8)  // primary text
#define UI_COLOR_INK2   lv_color_hex(0xBEC3D6)  // secondary text
#define UI_COLOR_INK3   lv_color_hex(0x8B8FA8)  // label / hint
#define UI_COLOR_INK4   lv_color_hex(0x606278)  // subtle / disabled

// Semantic
#define UI_COLOR_GOOD   lv_color_hex(0x6ECB7F)  // green  (cheap electricity, ok status)
#define UI_COLOR_WARN   lv_color_hex(0xE5C45A)  // amber  (average electricity, warning)
#define UI_COLOR_BAD    lv_color_hex(0xD85C3A)  // orange-red (expensive, alert)

// Domain sensor colors
#define UI_COLOR_TEMP   lv_color_hex(0xC87830)  // temperature arc (orange)
#define UI_COLOR_PRES   lv_color_hex(0x7DD4B0)  // pressure arc (teal)
#define UI_COLOR_HUM    lv_color_hex(0x6FB0D8)  // humidity arc (blue)

// Accent (cyan-blue, used for active tab, focus, battery arc)
#define UI_COLOR_ACCENT lv_color_hex(0x5B9FD4)

// Electricity price thresholds (chart value 0-120 scale)
#define UI_ELPRIS_CHEAP_MAX  40
#define UI_ELPRIS_WARN_MAX   80

#endif // UI_THEME_H
