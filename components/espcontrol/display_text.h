#pragma once

#include <algorithm>
#include <cstring>
#include <string>

#include "esphome/components/lvgl/lvgl_esphome.h"
#include "button_grid_string.h"

// Centered cards: the icon font draws a glyph inside a wider advance box and not always in
// the middle of it, so centering the label box leaves the visible icon off-centre. Return
// how many pixels to move the label so the drawn part is centred.
inline int centered_icon_dx(lv_obj_t *label) {
  if (label == nullptr) return 0;
  const char *text = lv_label_get_text(label);
  const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
  if (text == nullptr || text[0] == '\0' || font == nullptr) return 0;
  const unsigned char *c = reinterpret_cast<const unsigned char *>(text);
  uint32_t letter = c[0];
  if (c[0] >= 0xF0 && c[1] != 0 && c[2] != 0 && c[3] != 0) {
    letter = (static_cast<uint32_t>(c[0] & 0x07) << 18) | (static_cast<uint32_t>(c[1] & 0x3F) << 12) |
             (static_cast<uint32_t>(c[2] & 0x3F) << 6) | static_cast<uint32_t>(c[3] & 0x3F);
  } else if (c[0] >= 0xE0 && c[1] != 0 && c[2] != 0) {
    letter = (static_cast<uint32_t>(c[0] & 0x0F) << 12) | (static_cast<uint32_t>(c[1] & 0x3F) << 6) |
             static_cast<uint32_t>(c[2] & 0x3F);
  } else if (c[0] >= 0xC0 && c[1] != 0) {
    letter = (static_cast<uint32_t>(c[0] & 0x1F) << 6) | static_cast<uint32_t>(c[1] & 0x3F);
  }
  lv_font_glyph_dsc_t glyph;
  if (!lv_font_get_glyph_dsc(font, &glyph, letter, 0) || glyph.box_w == 0) return 0;
  const int line = lv_font_get_line_height(font);
  const int dx = static_cast<int>(glyph.adv_w) / 2 - (static_cast<int>(glyph.ofs_x) + static_cast<int>(glyph.box_w) / 2);
  return std::max(-line, std::min(line, dx));
}

inline void *centered_icon_marker() {
  static char marker;
  return &marker;
}

// Re-centre a centered-card icon after its glyph changed (the new glyph has its own ink offset).
inline void centered_icon_refresh(lv_obj_t *label) {
  if (label == nullptr || lv_obj_get_user_data(label) != centered_icon_marker()) return;
  lv_obj_set_x(label, centered_icon_dx(label));
}

// LVGL copies label text, so a normalized temporary remains valid for the
// duration of this call. Avoid allocating for the overwhelmingly common case.
inline void lv_label_set_display_text(lv_obj_t *label, const char *text) {
  if (text == nullptr ||
      (std::strstr(text, "\xE1\xB9\xA2") == nullptr &&
       std::strstr(text, "\xE1\xB9\xA3") == nullptr)) {
    lv_label_set_text(label, text);
    centered_icon_refresh(label);
    return;
  }
  const std::string normalized = normalize_display_text(text);
  lv_label_set_text(label, normalized.c_str());
  centered_icon_refresh(label);
}
