#!/usr/bin/env python3
"""
Patches lv_font_montserrat_12/14.c to add .fallback pointing to Swedish supplement.
Removes .fallback from the supplement files to avoid circular reference.
Idempotent — sentinel + comma check distinguish correct patch from broken one.
"""
import os
import re
import sys

SENTINEL = "/* SWEDISH_PATCH */"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LVGL_DIR = os.path.join(ROOT, "managed_components", "lvgl__lvgl", "src", "font")
SUPP_DIR = os.path.join(ROOT, "components", "fonts")

PAIRS = [
    ("lv_font_montserrat_14.c", "montserrat_14", "montserrat_14.c"),
    ("lv_font_montserrat_12.c", "montserrat_12", "montserrat_12.c"),
]


def strip_patch(text):
    """Remove any previous SWEDISH_PATCH from text (returns stripped text)."""
    text = re.sub(
        rf"{re.escape(SENTINEL)}\nextern const lv_font_t \w+;\n\n",
        "",
        text,
    )
    text = re.sub(
        r"#if LV_VERSION_CHECK\(8, 2, 0\)[^\n]+\n"
        r"    \.fallback = &\w+, /\* SWEDISH_PATCH \*/\n"
        r"#endif\n",
        "",
        text,
    )
    # Remove spurious comma left on .dsc line by a broken previous patch
    text = re.sub(r"(\.dsc = &font_dsc),(\s*/\*)", r"\1\2", text)
    return text


def patch_lvgl(path, name):
    text = open(path).read()

    # Already correctly patched: sentinel present AND comma added to .dsc line
    if SENTINEL in text and ".dsc = &font_dsc," in text:
        return

    # Broken patch present (sentinel without comma) — strip and redo
    if SENTINEL in text:
        text = strip_patch(text)

    # Insert extern before "Initialize a public general font descriptor" comment
    text = re.sub(
        r"(/\*Initialize a public general font descriptor\*/)",
        f"{SENTINEL}\nextern const lv_font_t {name};\n\n\\1",
        text,
    )

    # Add comma to .dsc line and insert .fallback before closing brace of font struct
    text = re.sub(
        r"(\.dsc = &font_dsc)([^\n]*\n)\};",
        r"\1,\2"
        f"#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9\n"
        f"    .fallback = &{name}, {SENTINEL}\n"
        f"#endif\n"
        r"};",
        text,
    )

    open(path, "w").write(text)
    print(f"[patch_fonts] patched {os.path.basename(path)}")


def unpatch_supplement(path):
    original = open(path).read()
    text = original

    # Remove: extern const lv_font_t lv_font_montserrat_NN;
    text = re.sub(r"extern const lv_font_t lv_font_montserrat_\d+;\n", "", text)

    # Remove: #    if LV_VERSION_CHECK(8, 2, 0) ... / .fallback = ... / #    endif
    text = re.sub(
        r"#\s+if LV_VERSION_CHECK\(8, 2, 0\)[^\n]+\n"
        r"\s+\.fallback = &lv_font_montserrat_\d+,\n"
        r"#\s+endif\n",
        "",
        text,
    )

    if text == original:
        return
    open(path, "w").write(text)
    print(f"[patch_fonts] removed fallback from {os.path.basename(path)}")


for lvgl_f, name, supp_f in PAIRS:
    lvgl_path = os.path.join(LVGL_DIR, lvgl_f)
    supp_path = os.path.join(SUPP_DIR, supp_f)

    if os.path.exists(lvgl_path):
        patch_lvgl(lvgl_path, name)
    else:
        print(f"[patch_fonts] WARNING: {lvgl_path} not found", file=sys.stderr)

    if os.path.exists(supp_path):
        unpatch_supplement(supp_path)
    else:
        print(f"[patch_fonts] WARNING: {supp_path} not found", file=sys.stderr)


def verify_patch(lvgl_path, name, supp_path):
    ok = True
    if os.path.exists(lvgl_path):
        text = open(lvgl_path).read()
        has_sentinel = SENTINEL in text
        has_comma    = ".dsc = &font_dsc," in text
        has_fallback = f".fallback = &{name}" in text
        if has_sentinel and has_comma and has_fallback:
            print(f"[patch_fonts] OK   {os.path.basename(lvgl_path)}: fallback → {name}")
        else:
            print(
                f"[patch_fonts] FAIL {os.path.basename(lvgl_path)}: "
                f"sentinel={has_sentinel} comma={has_comma} fallback={has_fallback}",
                file=sys.stderr,
            )
            ok = False
    else:
        print(f"[patch_fonts] FAIL {os.path.basename(lvgl_path)}: not found", file=sys.stderr)
        ok = False

    if os.path.exists(supp_path):
        supp = open(supp_path).read()
        circular = bool(re.search(r"\.fallback\s*=\s*&lv_font_montserrat_\d+", supp))
        if circular:
            print(
                f"[patch_fonts] WARN {os.path.basename(supp_path)}: circular fallback still present!",
                file=sys.stderr,
            )
        else:
            print(f"[patch_fonts] OK   {os.path.basename(supp_path)}: no circular fallback")
    return ok


print("[patch_fonts] --- verification ---")
all_ok = all(
    verify_patch(
        os.path.join(LVGL_DIR, lvgl_f),
        name,
        os.path.join(SUPP_DIR, supp_f),
    )
    for lvgl_f, name, supp_f in PAIRS
)
sys.exit(0 if all_ok else 1)
