/*******************************************************************************
 * Size: 12 px
 * Bpp: 4
 * Opts: --bpp 4 --size 12 --no-compress --no-prefilter --font
 *managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf --symbols åÅöÖäÄ
 *--format lvgl -o docs/Montserrat/montserrat_12.c
 ******************************************************************************/

#ifdef __has_include
#    if __has_include("lvgl.h")
#        ifndef LV_LVGL_H_INCLUDE_SIMPLE
#            define LV_LVGL_H_INCLUDE_SIMPLE
#        endif
#    endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#    include "lvgl.h"
#else
#    include "lvgl/lvgl.h"
#endif

#ifndef MONTSERRAT_12
#    define MONTSERRAT_12 1
#endif

#if MONTSERRAT_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t GLYPH_BITMAP[] = {
    /* U+00C4 "Ä" */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0xc0, 0xe0, 0x0, 0x0, 0x0, 0x20, 0x20, 0x0, 0x0, 0x0, 0x6f,
    0x30, 0x0, 0x0, 0x0, 0xdd, 0x90, 0x0, 0x0, 0x4, 0xe3, 0xf1, 0x0, 0x0, 0xb, 0x80, 0xc7, 0x0, 0x0,
    0x1f, 0x20, 0x6e, 0x0, 0x0, 0x8c, 0x0, 0x1f, 0x50, 0x0, 0xef, 0xee, 0xef, 0xb0, 0x6, 0xe2, 0x11,
    0x14, 0xf2, 0xc, 0x70, 0x0, 0x0, 0xb9,

    /* U+00C5 "Å" */
    0x0, 0x0, 0x69, 0x30, 0x0, 0x0, 0x0, 0x90, 0x90, 0x0, 0x0, 0x0, 0x59, 0x30, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x6f, 0x30, 0x0, 0x0, 0x0, 0xdd, 0x90, 0x0, 0x0, 0x4, 0xe3, 0xf1, 0x0, 0x0,
    0xb, 0x80, 0xc7, 0x0, 0x0, 0x1f, 0x20, 0x6e, 0x0, 0x0, 0x8c, 0x0, 0x1f, 0x50, 0x0, 0xef, 0xee,
    0xef, 0xb0, 0x6, 0xe2, 0x11, 0x14, 0xf2, 0xc, 0x70, 0x0, 0x0, 0xb9,

    /* U+00D6 "Ö" */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x9, 0x65, 0xa0, 0x0, 0x0, 0x1, 0x0, 0x10, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x3b, 0xef, 0xb4, 0x0, 0x5, 0xf9, 0x33, 0x8f, 0x60, 0xe, 0x60, 0x0, 0x5, 0xf1,
    0x4e, 0x0, 0x0, 0x0, 0xd5, 0x6c, 0x0, 0x0, 0x0, 0xb7, 0x4e, 0x0, 0x0, 0x0, 0xd5, 0xe, 0x60, 0x0,
    0x5, 0xf1, 0x5, 0xf9, 0x33, 0x8f, 0x60, 0x0, 0x3b, 0xef, 0xb4, 0x0,

    /* U+00E4 "ä" */
    0x2, 0xc0, 0xd1, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8d, 0xfc, 0x30, 0xa, 0x42, 0x9d, 0x0,
    0x0, 0x1, 0xf1, 0x8, 0xde, 0xef, 0x24, 0xe1, 0x0, 0xf2, 0x4e, 0x0, 0x7f, 0x20, 0x9e, 0xd8, 0xf2,

    /* U+00E5 "å" */
    0x0, 0x49, 0x40, 0x0, 0x9, 0x9, 0x0, 0x0, 0x49, 0x40, 0x0, 0x0, 0x0, 0x0, 0x8, 0xdf, 0xc3, 0x0,
    0xa4, 0x29, 0xd0, 0x0, 0x0, 0x1f, 0x10, 0x8d, 0xee, 0xf2, 0x4e, 0x10, 0xf, 0x24, 0xe0, 0x7,
    0xf2, 0x9, 0xed, 0x8f, 0x20,

    /* U+00F6 "ö" */
    0x0, 0xc2, 0x86, 0x0, 0x0, 0x30, 0x21, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0xbf, 0xe8, 0x0, 0xe, 0xa2,
    0x3e, 0x80, 0x5d, 0x0, 0x4, 0xf0, 0x7b, 0x0, 0x1, 0xf1, 0x5d, 0x0, 0x4, 0xf0, 0xd, 0xa2, 0x3e,
    0x80, 0x2, 0xbf, 0xe8, 0x0};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t GLYPH_DSC[] = {
    {.bitmap_index = 0,
     .adv_w        = 0,
     .box_w        = 0,
     .box_h        = 0,
     .ofs_x        = 0,
     .ofs_y        = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 141, .box_w = 10, .box_h = 12, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 141, .box_w = 10, .box_h = 13, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 161, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 115, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 115, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 264, .adv_w = 122, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0}};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t UNICODE_LIST_0[] = {0x0, 0x1, 0x12, 0x20, 0x21, 0x32};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t CMAPS[] = {{.range_start       = 196,
                                                .range_length      = 51,
                                                .glyph_id_start    = 1,
                                                .unicode_list      = UNICODE_LIST_0,
                                                .glyph_id_ofs_list = NULL,
                                                .list_length       = 6,
                                                .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}};

/*-----------------
 *    KERNING
 *----------------*/

/*Map glyph_ids to kern left classes*/
static const uint8_t KERN_LEFT_CLASS_MAPPING[] = {0, 1, 1, 2, 3, 3, 4};

/*Map glyph_ids to kern right classes*/
static const uint8_t KERN_RIGHT_CLASS_MAPPING[] = {0, 1, 1, 2, 3, 3, 4};

/*Kern values between classes*/
static const int8_t KERN_CLASS_VALUES[] = {2, -2, 0, -1, -2, 0, 0, 0, 0, 0, 0, 1, -2, 0, -1, 0};

/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t KERN_CLASSES = {
    .class_pair_values   = KERN_CLASS_VALUES,
    .left_class_mapping  = KERN_LEFT_CLASS_MAPPING,
    .right_class_mapping = KERN_RIGHT_CLASS_MAPPING,
    .left_class_cnt      = 4,
    .right_class_cnt     = 4,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#    if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#    endif

#    if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t FONT_DSC = {
#    else
static lv_font_fmt_txt_dsc_t font_dsc = {
#    endif
    .glyph_bitmap  = GLYPH_BITMAP,
    .glyph_dsc     = GLYPH_DSC,
    .cmaps         = CMAPS,
    .kern_dsc      = &KERN_CLASSES,
    .kern_scale    = 16,
    .cmap_num      = 1,
    .bpp           = 4,
    .kern_classes  = 1,
    .bitmap_format = 0,
#    if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#    endif
};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

extern const lv_font_t lv_font_montserrat_12;

/*Initialize a public general font descriptor*/
#    if LVGL_VERSION_MAJOR >= 8
const lv_font_t montserrat_12 = {
#    else
lv_font_t montserrat_12 = {
#    endif
    .get_glyph_dsc    = lv_font_get_glyph_dsc_fmt_txt, /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height      = 13, /*The maximum line height required by the font*/
    .base_line        = 0,  /*Baseline measured from the bottom of the line*/
#    if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#    endif
#    if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position  = -1,
    .underline_thickness = 1,
#    endif
    .dsc = &FONT_DSC, /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#    if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_montserrat_12,
#    endif
    .user_data = NULL,
};

#endif /*#if MONTSERRAT_12*/
