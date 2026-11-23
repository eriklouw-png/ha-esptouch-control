#pragma once

#include "card_availability.h"

// Internal implementation detail for button_grid.h. Include button_grid.h from device YAML.

// ── Home Assistant subscriptions ──────────────────────────────────────

// The grid owns device colour correction; callbacks sample the live neutral
// instead of retaining the palette that was active when they subscribed.
inline uint32_t current_grid_sensor_color();
inline uint32_t current_grid_sensor_surface_color();

struct ToggleTextSensorCtx {
  lv_obj_t *text_lbl = nullptr;
  std::string steady_text;
  std::string sensor_text = "--";
  bool on = false;
};

struct TimeSensorCtx {
  lv_obj_t *sensor_lbl = nullptr;
  lv_obj_t *unit_lbl = nullptr;
  std::string entity_id;
  std::string manual_unit;
  std::string state;
  std::string auto_unit;
  bool has_state = false;
  bool has_auto_unit = false;
  int max_components = 2;
  std::string warned_unit;
  bool warned_unit_set = false;
};

inline std::string label_text_or_empty(lv_obj_t *label) {
  if (!label) return "";
  const char *text = lv_label_get_text(label);
  return text ? std::string(text) : "";
}

inline void apply_toggle_text_sensor_label(ToggleTextSensorCtx *ctx) {
  if (!ctx || !ctx->text_lbl) return;
  set_wrapped_button_label_text(ctx->text_lbl, ctx->on ? ctx->sensor_text : ctx->steady_text);
}

inline void apply_sensor_active_color(lv_obj_t *btn, bool active_color,
                                      esphome::StringRef state,
                                      uint32_t on_color, uint32_t sensor_color,
                                      bool unavailable,
                                      bool numeric_mode = false,
                                      bool custom_background = false) {
  if (!btn || !active_color) return;
  const bool active = !unavailable && sensor_active_color_state_ref(state, numeric_mode);
  theme_set_content_background(btn, active || custom_background);
  uint32_t next_color = active ? on_color : sensor_color;
  lv_obj_set_style_bg_color(btn, lv_color_hex(next_color),
    static_cast<lv_style_selector_t>(LV_PART_MAIN) | static_cast<lv_style_selector_t>(LV_STATE_DEFAULT));
  lv_obj_set_style_text_color(btn, lv_color_hex(active || custom_background ? display_text_color_for_bg(next_color) : current_theme().text_primary),
    static_cast<lv_style_selector_t>(LV_PART_MAIN) | static_cast<lv_style_selector_t>(LV_STATE_DEFAULT));
  sync_card_checked_text_color(btn);
}

inline void apply_control_availability(lv_obj_t *visual_obj, lv_obj_t *input_obj,
                                       bool available) {
  if (visual_obj) {
    set_card_disabled_state(visual_obj, !available);
  }
  if (input_obj && input_obj != visual_obj) {
    if (available) lv_obj_clear_state(input_obj, LV_STATE_DISABLED);
    else lv_obj_add_state(input_obj, LV_STATE_DISABLED);
  }
  if (!input_obj) return;
  if (available) lv_obj_add_flag(input_obj, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_clear_flag(input_obj, LV_OBJ_FLAG_CLICKABLE);
}

inline void register_ha_control_availability(lv_obj_t *visual_obj, lv_obj_t *input_obj) {
  apply_control_availability(visual_obj, input_obj, true);
}

inline void subscribe_control_availability(lv_obj_t *visual_obj, lv_obj_t *input_obj,
                                           const std::string &entity_id) {
  register_ha_control_availability(visual_obj, input_obj);
  if (entity_id.empty()) return;
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>(
      [visual_obj, input_obj](esphome::StringRef state) {
        apply_control_availability(visual_obj, input_obj, !ha_state_unavailable_ref(state));
      })
  );
}

// Subscribe to a HA sensor entity and update an LVGL label with its numeric value.
inline void subscribe_sensor_value(lv_obj_t *sensor_lbl, const std::string &sensor_id,
                                   int precision = 0,
                                   lv_obj_t *unit_lbl = nullptr,
                                   const std::string &unit = "",
                                   lv_obj_t *availability_obj = nullptr,
                                   bool active_color = false,
                                   uint32_t on_color = DEFAULT_ACCENT_COLOR,
                                   bool sensor_surface = false,
                                   uint32_t custom_sensor_color = UINT32_MAX) {
  std::string display_unit = trim_display_unit(unit);
  ha_subscribe_state(
    sensor_id,
    std::function<void(esphome::StringRef)>(
      [sensor_lbl, precision, unit_lbl, display_unit, availability_obj,
       active_color, on_color, sensor_surface, custom_sensor_color](esphome::StringRef state) {
      bool unavailable = ha_state_unavailable_ref(state);
      apply_sensor_active_color(availability_obj, active_color, state,
        on_color, custom_sensor_color != UINT32_MAX ? custom_sensor_color
          : (sensor_surface ? current_grid_sensor_surface_color() : current_grid_sensor_color()),
        unavailable, true, custom_sensor_color != UINT32_MAX);

      float val = 0.0f;
      if (!unavailable && parse_float_ref(state, val) && std::isfinite(val)) {
        char buf[16];
        format_fixed_decimal(buf, sizeof(buf), val, precision);
        lv_label_set_display_text(sensor_lbl, buf);
        if (unit_lbl) lv_label_set_display_text(unit_lbl, display_unit.c_str());
      } else {
        lv_label_set_display_text(sensor_lbl, "");
        if (unit_lbl) lv_label_set_display_text(unit_lbl, "");
      }
    })
  );
}

inline void apply_time_sensor_value(TimeSensorCtx *ctx) {
  if (!ctx || !ctx->sensor_lbl) return;
  if (ctx->unit_lbl) lv_label_set_display_text(ctx->unit_lbl, "");
  if (!ctx->has_state || (ctx->manual_unit.empty() && !ctx->has_auto_unit)) {
    lv_label_set_display_text(ctx->sensor_lbl, "");
    return;
  }

  const std::string &input_unit = ctx->manual_unit.empty() ? ctx->auto_unit : ctx->manual_unit;
  double multiplier = 0.0;
  if (!duration_unit_seconds_multiplier(input_unit, multiplier)) {
    lv_label_set_display_text(ctx->sensor_lbl, "");
    if (ctx->manual_unit.empty() &&
        (!ctx->warned_unit_set || ctx->warned_unit != input_unit)) {
      ESP_LOGW("sensors", "Time sensor %s has unsupported or missing unit_of_measurement '%s'",
               ctx->entity_id.c_str(), input_unit.c_str());
      ctx->warned_unit = input_unit;
      ctx->warned_unit_set = true;
    }
    return;
  }
  ctx->warned_unit.clear();
  ctx->warned_unit_set = false;

  char output[48];
  if (!format_duration_sensor_state(output, sizeof(output),
                                    ctx->state, ctx->has_state,
                                    ctx->auto_unit, ctx->has_auto_unit,
                                    ctx->manual_unit, ctx->max_components)) {
    lv_label_set_display_text(ctx->sensor_lbl, "");
    return;
  }
  lv_label_set_display_text(ctx->sensor_lbl, output);
}

inline void subscribe_time_sensor_value(TimeSensorCtx *ctx, const std::string &sensor_id,
                                        const std::string &manual_unit,
                                        int max_components = 2) {
  if (!ctx) return;
  ctx->entity_id = sensor_id;
  ctx->manual_unit = manual_unit;
  ctx->max_components = max_components;
  if (ctx->unit_lbl) lv_label_set_display_text(ctx->unit_lbl, "");
  ha_subscribe_state(
    sensor_id,
    std::function<void(esphome::StringRef)>([ctx](esphome::StringRef state) {
      if (!ctx) return;
      ctx->state.assign(state.c_str(), state.size());
      ctx->has_state = true;
      apply_time_sensor_value(ctx);
    })
  );
  if (!manual_unit.empty()) return;
  ha_subscribe_attribute(
    sensor_id, "unit_of_measurement",
    std::function<void(esphome::StringRef)>([ctx](esphome::StringRef unit) {
      if (!ctx) return;
      ctx->auto_unit.assign(unit.c_str(), unit.size());
      ctx->has_auto_unit = true;
      apply_time_sensor_value(ctx);
    })
  );
}

inline void subscribe_toggle_text_sensor_value(ToggleTextSensorCtx *ctx, const std::string &sensor_id) {
  ha_subscribe_state(
    sensor_id,
    std::function<void(esphome::StringRef)>([ctx](esphome::StringRef state) {
      if (!ctx) return;
      ctx->sensor_text = text_sensor_display_text(state);
      apply_toggle_text_sensor_label(ctx);
    })
  );
}

inline void subscribe_text_sensor_value(lv_obj_t *text_lbl, const std::string &sensor_id,
                                        lv_obj_t *availability_obj = nullptr,
                                        bool active_color = false,
                                        uint32_t on_color = DEFAULT_ACCENT_COLOR,
                                        bool sensor_surface = false,
                                        uint32_t custom_sensor_color = UINT32_MAX) {
  ha_subscribe_state(
    sensor_id,
    std::function<void(esphome::StringRef)>(
      [text_lbl, availability_obj, active_color, on_color, sensor_surface, custom_sensor_color](esphome::StringRef state) {
      bool unavailable = ha_state_unavailable_ref(state);
      apply_sensor_active_color(availability_obj, active_color, state,
        on_color, custom_sensor_color != UINT32_MAX ? custom_sensor_color
          : (sensor_surface ? current_grid_sensor_surface_color() : current_grid_sensor_color()),
        unavailable, false, custom_sensor_color != UINT32_MAX);
      set_wrapped_button_label_text(text_lbl, text_sensor_display_text(state));
    })
  );
}

inline void subscribe_sensor_icon_state(lv_obj_t *btn_ptr, lv_obj_t *icon_lbl,
                                        const ParsedCfg &p,
                                        bool active_color = false) {
  if (p.sensor.empty()) return;
  const char *icon_off = (p.icon.empty() || p.icon == "Auto")
    ? find_icon("Auto") : find_icon(p.icon.c_str());
  bool has_icon_on = !p.icon_on.empty() && p.icon_on != "Auto";
  const char *icon_on = has_icon_on ? find_icon(p.icon_on.c_str()) : icon_off;
  lv_label_set_display_text(icon_lbl, find_icon("Minus"));
  set_card_disabled_state(btn_ptr, true);
  ha_subscribe_state(
    p.sensor,
    std::function<void(esphome::StringRef)>(
      [btn_ptr, icon_lbl, icon_off, icon_on, active_color](esphome::StringRef state) {
      bool unavailable = ha_state_unavailable_ref(state);
      set_card_disabled_state(btn_ptr, unavailable);
      if (btn_ptr) {
        set_card_checked_state(
          btn_ptr, active_color && !unavailable && is_entity_on_ref(state));
      }
      lv_label_set_display_text(icon_lbl, unavailable
        ? find_icon("Minus") : (is_entity_on_ref(state) ? icon_on : icon_off));
    })
  );
}

inline void subscribe_sensor_text_card_value(lv_obj_t *text_lbl, const ParsedCfg &p,
                                             lv_obj_t *availability_obj = nullptr,
                                             bool active_color = false,
                                             uint32_t on_color = DEFAULT_ACCENT_COLOR,
                                             bool sensor_surface = true,
                                             uint32_t custom_sensor_color = UINT32_MAX) {
  if (p.sensor.empty()) return;
  ha_subscribe_state(
    p.sensor,
    std::function<void(esphome::StringRef)>(
      [text_lbl, p, availability_obj, active_color, on_color, sensor_surface, custom_sensor_color](esphome::StringRef state) {
      bool unavailable = ha_state_unavailable_ref(state);
      apply_sensor_active_color(availability_obj, active_color, state,
        on_color, custom_sensor_color != UINT32_MAX ? custom_sensor_color
          : (sensor_surface ? current_grid_sensor_surface_color() : current_grid_sensor_color()),
        unavailable, false, custom_sensor_color != UINT32_MAX);
      set_wrapped_button_label_text(text_lbl, sensor_state_display_text(p, state));
    })
  );
}

inline void subscribe_weather_state(lv_obj_t *icon_lbl, lv_obj_t *text_lbl, const std::string &entity_id) {
  ESP_LOGI("weather", "Subscribing to current weather state for %s", entity_id.c_str());
  uint32_t generation = ha_subscription_generation();
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>([icon_lbl, text_lbl, entity_id, generation](esphome::StringRef state) {
      if (generation != ha_subscription_generation()) return;
      std::string state_text = string_ref_limited(state, HA_SHORT_STATE_MAX_LEN);
      ESP_LOGI("weather", "Current weather state for %s: %s", entity_id.c_str(), state_text.c_str());
      lv_label_set_display_text(icon_lbl, weather_icon_for_state(state_text));
      lv_label_set_display_text(text_lbl, weather_label_for_state(state_text).c_str());
      notify_dashboard_content_changed();
    })
  );
}

inline void subscribe_garage_state(lv_obj_t *btn_ptr, lv_obj_t *icon_lbl,
                                   TransientStatusLabel *status_label,
                                   const char *closed_icon, const char *open_icon,
                                   const std::string &entity_id,
                                   bool persistent_status = false) {
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>(
      [btn_ptr, icon_lbl, status_label, closed_icon, open_icon, persistent_status](esphome::StringRef state) {
        std::string state_text = string_ref_limited(state, HA_SHORT_STATE_MAX_LEN);
        bool active = garage_state_is_active(state_text);
        set_card_checked_state(btn_ptr, active);
        lv_label_set_display_text(icon_lbl, garage_state_uses_open_icon(state_text) ? open_icon : closed_icon);
        transient_status_label_show_if_changed(
          status_label, garage_state_label(state_text),
          persistent_status ? false : garage_state_releases_label(state_text));
      })
  );
}

inline void subscribe_gate_state(lv_obj_t *btn_ptr, lv_obj_t *icon_lbl,
                                 TransientStatusLabel *status_label,
                                 const char *closed_icon, const char *open_icon,
                                 const std::string &entity_id,
                                 bool persistent_status = false) {
  register_ha_control_availability(btn_ptr, btn_ptr);
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>(
      [btn_ptr, icon_lbl, status_label, closed_icon, open_icon, persistent_status](esphome::StringRef state) {
        std::string state_text = string_ref_limited(state, HA_SHORT_STATE_MAX_LEN);
        bool unavailable = ha_state_unavailable_ref(state);
        apply_control_availability(btn_ptr, btn_ptr, !unavailable);
        bool active = garage_state_is_active(state_text);
        set_card_checked_state(btn_ptr, active);
        lv_label_set_display_text(icon_lbl, garage_state_uses_open_icon(state_text) ? open_icon : closed_icon);
        transient_status_label_show_if_changed(
          status_label, garage_state_label(state_text),
          persistent_status ? false : garage_state_releases_label(state_text));
      })
  );
}

inline void subscribe_cover_toggle_state(lv_obj_t *btn_ptr, lv_obj_t *icon_lbl,
                                         TransientStatusLabel *status_label,
                                         const char *closed_icon, const char *open_icon,
                                         const std::string &entity_id) {
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>(
      [btn_ptr, icon_lbl, status_label, closed_icon, open_icon](esphome::StringRef state) {
        std::string state_text = string_ref_limited(state, HA_SHORT_STATE_MAX_LEN);
        bool active = cover_toggle_state_is_active(state_text);
        set_card_checked_state(btn_ptr, active);
        lv_label_set_display_text(icon_lbl, garage_state_uses_open_icon(state_text) ? open_icon : closed_icon);
        transient_status_label_show_if_changed(
          status_label, garage_state_label(state_text), garage_state_releases_label(state_text));
      })
  );
}

inline void subscribe_lock_state(lv_obj_t *btn_ptr, lv_obj_t *icon_lbl,
                                 TransientStatusLabel *status_label,
                                 const char *locked_icon, const char *unlocked_icon,
                                 LockCardCtx *ctx) {
  if (!ctx) return;
  ha_subscribe_state(
    ctx->entity_id,
    std::function<void(esphome::StringRef)>(
      [btn_ptr, icon_lbl, status_label, locked_icon, unlocked_icon, ctx](esphome::StringRef state) {
        std::string state_text = string_ref_limited(state, HA_SHORT_STATE_MAX_LEN);
        ctx->state = state_text;
        bool active = lock_state_is_active(state_text);
        set_card_checked_state(btn_ptr, active);
        lv_label_set_display_text(icon_lbl,
          lock_state_uses_unlocked_icon(state_text) ? unlocked_icon : locked_icon);
        transient_status_label_show_if_changed(
          status_label, lock_state_label(state_text), lock_state_releases_label(state_text));
      })
  );
}

// Subscribe to an entity's friendly_name attribute and use it as the button label
inline void subscribe_friendly_name(TransientStatusLabel *status_label,
                                    const std::string &entity_id) {
  ha_subscribe_attribute(
    entity_id, std::string("friendly_name"),
    std::function<void(esphome::StringRef)>([status_label](esphome::StringRef name) {
      transient_status_label_set_steady(status_label, string_ref_limited(name, HA_FRIENDLY_NAME_MAX_LEN));
    })
  );
}

inline void subscribe_friendly_name(ToggleTextSensorCtx *ctx,
                                    const std::string &entity_id) {
  ha_subscribe_attribute(
    entity_id, std::string("friendly_name"),
    std::function<void(esphome::StringRef)>([ctx](esphome::StringRef name) {
      if (!ctx) return;
      ctx->steady_text = string_ref_limited(name, HA_FRIENDLY_NAME_MAX_LEN);
      if (!ctx->on) apply_toggle_text_sensor_label(ctx);
    })
  );
}

inline void subscribe_friendly_name(lv_obj_t *text_lbl, const std::string &entity_id) {
  ha_subscribe_attribute(
    entity_id, std::string("friendly_name"),
    std::function<void(esphome::StringRef)>([text_lbl](esphome::StringRef name) {
      set_wrapped_button_label_text(text_lbl, string_ref_limited(name, HA_FRIENDLY_NAME_MAX_LEN));
    })
  );
}

// Slider cards zero the button padding so their fill can reach the tile edges,
// then position the label with a captured inset. Updating the friendly name
// must not re-anchor that label at (0, 0), or it becomes flush with the tile.
inline void subscribe_friendly_name_preserving_layout(
    lv_obj_t *text_lbl, const std::string &entity_id) {
  ha_subscribe_attribute(
    entity_id, std::string("friendly_name"),
    std::function<void(esphome::StringRef)>([text_lbl](esphome::StringRef name) {
      if (!text_lbl) return;
      configure_button_label_wrap(text_lbl);
      lv_label_set_display_text(
        text_lbl, string_ref_limited(name, HA_FRIENDLY_NAME_MAX_LEN).c_str());
    })
  );
}

// Subscribe to a toggle entity's state; updates checked visual, icon swap, sensor overlay
inline void subscribe_toggle_state(lv_obj_t *btn_ptr, lv_obj_t *icon_lbl,
                                   lv_obj_t *sensor_ctr, lv_obj_t *text_lbl,
                                   bool *slot_has_sensor, bool *slot_sensor_text_mode,
                                   bool *slot_has_icon_on,
                                   const char **slot_icon_off, const char **slot_icon_on,
                                   ToggleTextSensorCtx *text_sensor_ctx,
                                   const std::string &entity_id,
                                   bool disable_interaction = true,
                                   bool (*is_active_state)(esphome::StringRef) = is_entity_on_ref) {
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>(
      [btn_ptr, icon_lbl, sensor_ctr, text_lbl, slot_has_sensor, slot_sensor_text_mode,
       slot_has_icon_on, slot_icon_off, slot_icon_on, text_sensor_ctx,
       is_active_state](esphome::StringRef state) {
        bool on = is_active_state(state);
        set_card_checked_state(btn_ptr, on);

        if (text_sensor_ctx) {
          text_sensor_ctx->on = on;
          apply_toggle_text_sensor_label(text_sensor_ctx);
        }

        bool show_numeric_sensor = *slot_has_sensor && !*slot_sensor_text_mode;
        if (show_numeric_sensor && sensor_ctr) {
          if (on) {
            lv_obj_add_flag(icon_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(sensor_ctr, LV_OBJ_FLAG_HIDDEN);
          } else {
            lv_obj_clear_flag(icon_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(sensor_ctr, LV_OBJ_FLAG_HIDDEN);
          }
        } else {
          if (icon_lbl) lv_obj_clear_flag(icon_lbl, LV_OBJ_FLAG_HIDDEN);
          if (sensor_ctr) lv_obj_add_flag(sensor_ctr, LV_OBJ_FLAG_HIDDEN);
          if (*slot_has_icon_on)
            lv_label_set_display_text(icon_lbl, on ? *slot_icon_on : *slot_icon_off);
        }
        configure_button_label_wrap(text_lbl);
      })
  );
}

// One entry of "Look Per State": how the card looks while its state entity has this state.
struct ActionCardLook {
  std::string state;
  const char *icon = nullptr;
  bool has_color = false;
  uint32_t color = 0;
};

inline std::string action_card_look_trim(const std::string &text) {
  size_t begin = 0, end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
  return text.substr(begin, end - begin);
}

inline bool action_card_look_same_state(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

// Parses "State:Icon:RRGGBB;State:Icon:RRGGBB"; icon and colour are optional.
inline std::vector<ActionCardLook> parse_action_card_looks(const std::string &spec) {
  std::vector<ActionCardLook> looks;
  size_t pos = 0;
  while (pos <= spec.size()) {
    size_t end = spec.find(';', pos);
    if (end == std::string::npos) end = spec.size();
    const std::string item = spec.substr(pos, end - pos);
    pos = end + 1;
    if (action_card_look_trim(item).empty() || action_card_look_trim(item)[0] == '@') continue;
    std::string parts[3];
    size_t part = 0, start = 0;
    for (size_t i = 0; i <= item.size() && part < 3; ++i) {
      if (i == item.size() || (item[i] == ':' && part < 2)) {
        parts[part++] = action_card_look_trim(item.substr(start, i - start));
        start = i + 1;
      }
    }
    ActionCardLook look;
    look.state = parts[0];
    if (look.state.empty()) continue;
    if (!parts[1].empty() && parts[1] != "Auto") look.icon = find_icon(parts[1].c_str());
    std::string hex = resolve_color_ref(parts[2]);
    if (!hex.empty() && hex[0] == '#') hex.erase(0, 1);
    if (hex.size() == 6) {
      char *tail = nullptr;
      const unsigned long value = std::strtoul(hex.c_str(), &tail, 16);
      if (tail != nullptr && *tail == '\0') {
        look.has_color = true;
        look.color = static_cast<uint32_t>(value);
      }
    }
    looks.push_back(look);
  }
  return looks;
}

// "@entity" or "@entity:unit" inside Look Per State: the left half of a wide status card shows
// the state of this entity (a number in a big font, anything else as small text).
inline void parse_action_card_info(const std::string &spec, std::string &entity, std::string &unit) {
  size_t pos = 0;
  while (pos <= spec.size()) {
    size_t end = spec.find(';', pos);
    if (end == std::string::npos) end = spec.size();
    const std::string item = action_card_look_trim(spec.substr(pos, end - pos));
    pos = end + 1;
    if (item.size() < 2 || item[0] != '@') continue;
    const size_t colon = item.find(':');
    entity = action_card_look_trim(item.substr(1, colon == std::string::npos ? std::string::npos : colon - 1));
    unit = colon == std::string::npos ? "" : action_card_look_trim(item.substr(colon + 1));
    return;
  }
}

struct ActionCardStateCtx {
  std::vector<ActionCardLook> looks;
  lv_obj_t *btn = nullptr;
  lv_obj_t *icon_lbl = nullptr;
  lv_obj_t *text_lbl = nullptr;
  lv_obj_t *sensor_lbl = nullptr;
  lv_obj_t *unit_lbl = nullptr;
  lv_obj_t *status_icon_lbl = nullptr;
  lv_obj_t *status_text_lbl = nullptr;
  lv_obj_t *status_value_lbl = nullptr;
  std::string info_entity;
  std::string info_unit;
  bool show_icon_state = false;
  bool show_text_state = false;
  bool show_numeric_state = false;
  bool has_icon_on = false;
  const char *icon_off = nullptr;
  const char *icon_on = nullptr;
  int precision = 0;
  std::string unit;
};

inline ActionCardStateCtx *create_action_card_state_context(const BtnSlot &s,
                                                            const ParsedCfg &p) {
  ActionCardStateCtx *ctx = new ActionCardStateCtx();
  ctx->btn = s.btn;
  ctx->icon_lbl = s.icon_lbl;
  ctx->text_lbl = s.text_lbl;
  ctx->sensor_lbl = s.sensor_lbl;
  ctx->unit_lbl = s.unit_lbl;
  ctx->show_icon_state = action_card_state_icon_mode(p);
  ctx->show_text_state = action_card_state_text_mode(p);
  ctx->show_numeric_state = action_card_state_numeric_mode(p);
  ctx->has_icon_on = !p.icon_on.empty() && p.icon_on != "Auto";
  ctx->icon_off = (p.icon.empty() || p.icon == "Auto") ? find_icon("Flash") : find_icon(p.icon.c_str());
  ctx->icon_on = ctx->has_icon_on ? find_icon(p.icon_on.c_str()) : ctx->icon_off;
  ctx->precision = parse_precision(action_card_state_precision(p));
  ctx->unit = trim_display_unit(action_card_state_unit(p));
  ctx->looks = parse_action_card_looks(action_card_state_look(p));
  parse_action_card_info(action_card_state_look(p), ctx->info_entity, ctx->info_unit);
  return ctx;
}

// Wide action card with a state entity and "look per state". Left half: title and the value
// of an optional info entity right under it (big for numbers), stacked tightly and centred.
// A thin divider in the middle. Right half, like any other card: icon of the current state on
// top, its text below, centred. Decided from the real card size, so it works for any layout.
inline void *action_card_status_container_marker() { static char m; return &m; }
inline void *action_card_status_value_marker() { static char m; return &m; }

inline lv_obj_t *action_card_find_marked_child(lv_obj_t *btn, void *marker) {
  if (!btn) return nullptr;
  const int32_t count = static_cast<int32_t>(lv_obj_get_child_cnt(btn));
  for (int32_t i = 0; i < count; i++) {
    lv_obj_t *child = lv_obj_get_child(btn, i);
    if (child && lv_obj_get_user_data(child) == marker) return child;
  }
  return nullptr;
}

inline lv_obj_t *action_card_plain_container(lv_obj_t *parent) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(box, 0, LV_PART_MAIN);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  return box;
}

// Title and value as one tight block, centred in the left half of the card.
inline void action_card_status_arrange(ActionCardStateCtx *ctx) {
  if (!ctx || !ctx->btn || !ctx->text_lbl || !ctx->status_value_lbl) return;
  lv_obj_update_layout(ctx->btn);
  const int w = lv_obj_get_width(ctx->btn);
  const int half = w / 2 - 12;
  lv_obj_t *title = ctx->text_lbl;
  lv_obj_t *value = ctx->status_value_lbl;
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title, half);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(value, half);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_update_layout(ctx->btn);
  const int h1 = lv_obj_get_height(title);
  const bool has_value = !ctx->info_entity.empty();
  const int h2 = has_value ? lv_obj_get_height(value) : 0;
  const int gap = has_value ? 2 : 0;
  const int total = h1 + gap + h2;
  lv_obj_align(title, LV_ALIGN_CENTER, -w / 4, -total / 2 + h1 / 2);
  if (has_value) lv_obj_align(value, LV_ALIGN_CENTER, -w / 4, total / 2 - h2 / 2);
  else lv_obj_add_flag(value, LV_OBJ_FLAG_HIDDEN);
}

inline bool action_card_status_layout(ActionCardStateCtx *ctx) {
  if (!ctx || !ctx->btn || !ctx->icon_lbl || !ctx->text_lbl) return false;
  if (!ctx->show_text_state || ctx->looks.empty()) return false;
  lv_obj_t *right = action_card_find_marked_child(ctx->btn, action_card_status_container_marker());
  lv_obj_t *value = action_card_find_marked_child(ctx->btn, action_card_status_value_marker());
  if (!right || !value) {
    lv_obj_update_layout(ctx->btn);
    const int w = lv_obj_get_width(ctx->btn);
    const int h = lv_obj_get_height(ctx->btn);
    if (h <= 0 || w * 10 < h * 16) return false;  // only for wide cards
    const lv_font_t *icon_font = lv_obj_get_style_text_font(ctx->icon_lbl, LV_PART_MAIN);
    const lv_font_t *text_font = lv_obj_get_style_text_font(ctx->text_lbl, LV_PART_MAIN);
    lv_obj_add_flag(ctx->icon_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(ctx->icon_lbl, nullptr);
    value = lv_label_create(ctx->btn);
    lv_obj_set_user_data(value, action_card_status_value_marker());
    lv_obj_set_style_text_font(value, text_font, LV_PART_MAIN);
    lv_label_set_text(value, "");
    lv_obj_clear_flag(value, LV_OBJ_FLAG_CLICKABLE);
    // Divider in the middle.
    lv_obj_t *line = action_card_plain_container(ctx->btn);
    lv_obj_set_size(line, 2, lv_pct(70));
    lv_obj_set_style_bg_color(line, lv_obj_get_style_text_color(ctx->text_lbl, LV_PART_MAIN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(line, LV_OPA_40, LV_PART_MAIN);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, 0);
    // Right half: exactly the right half of the card, its content centred in it.
    right = action_card_plain_container(ctx->btn);
    lv_obj_set_user_data(right, action_card_status_container_marker());
    lv_obj_set_size(right, w / 2, lv_pct(100));
    lv_obj_align(right, LV_ALIGN_CENTER, w / 4, 0);
    lv_obj_t *icon = lv_label_create(right);
    lv_obj_t *text = lv_label_create(right);
    lv_obj_set_style_text_font(icon, icon_font, LV_PART_MAIN);
    lv_obj_set_style_text_font(text, text_font, LV_PART_MAIN);
    lv_label_set_text(icon, "");
    lv_label_set_text(text, "");
    lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
    lv_obj_set_width(text, w / 2 - 12);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, lv_font_get_line_height(icon_font) / 5);
    lv_obj_align(text, LV_ALIGN_BOTTOM_MID, 0, 0);
    // The icon font draws glyphs off-centre: every icon swap re-centres its own glyph.
    lv_obj_set_user_data(icon, centered_icon_marker());
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(text, LV_OBJ_FLAG_CLICKABLE);
    ctx->status_value_lbl = value;
    action_card_status_arrange(ctx);
  }
  ctx->status_icon_lbl = lv_obj_get_child(right, 0);
  ctx->status_text_lbl = lv_obj_get_child(right, 1);
  ctx->status_value_lbl = value;
  return true;
}

// Value of the info entity in the left half: a number in the big font, anything else small.
inline void apply_action_card_info_value(ActionCardStateCtx *ctx, esphome::StringRef state, bool unavailable) {
  if (!ctx || ctx->info_entity.empty() || !action_card_status_layout(ctx) || !ctx->status_value_lbl) return;
  float val = 0.0f;
  if (!unavailable && parse_float_ref(state, val) && std::isfinite(val)) {
    // Big number font (digits, '.', '-' and the degree sign only): one decimal when it fits
    // in the left half, otherwise whole numbers.
    const lv_font_t *big = ctx->sensor_lbl ? lv_obj_get_style_text_font(ctx->sensor_lbl, LV_PART_MAIN) : nullptr;
    if (big) lv_obj_set_style_text_font(ctx->status_value_lbl, big, LV_PART_MAIN);
    lv_label_set_long_mode(ctx->status_value_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ctx->status_value_lbl, LV_SIZE_CONTENT);
    const int available = lv_obj_get_width(ctx->btn) / 2 - 12;
    for (int decimals = 1; decimals >= 0; decimals--) {
      char buf[24];
      format_fixed_decimal(buf, sizeof(buf), val, decimals);
      std::string out(buf);
      out += ctx->info_unit;
      lv_label_set_display_text(ctx->status_value_lbl, out.c_str());
      lv_obj_update_layout(ctx->status_value_lbl);
      if (decimals == 0 || lv_obj_get_width(ctx->status_value_lbl) <= available) break;
    }
  } else {
    std::string out = unavailable ? std::string("--") : text_sensor_display_text(state, HA_STATE_TEXT_MAX_LEN);
    if (!out.empty() && out[0] >= 'a' && out[0] <= 'z') out[0] = static_cast<char>(out[0] - 'a' + 'A');
    lv_obj_set_style_text_font(ctx->status_value_lbl, lv_obj_get_style_text_font(ctx->text_lbl, LV_PART_MAIN), LV_PART_MAIN);
    lv_label_set_long_mode(ctx->status_value_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_display_text(ctx->status_value_lbl, out.c_str());
  }
  action_card_status_arrange(ctx);
}

inline void apply_action_card_display_value(ActionCardStateCtx *ctx,
                                            esphome::StringRef state,
                                            bool unavailable) {
  if (!ctx) return;
  action_card_status_layout(ctx);
  if (!ctx->looks.empty() && !unavailable) {
    const std::string current(state.c_str(), state.size());
    for (const ActionCardLook &look : ctx->looks) {
      if (!action_card_look_same_state(current, look.state)) continue;
      lv_obj_t *look_icon = ctx->status_icon_lbl ? ctx->status_icon_lbl : ctx->icon_lbl;
      if (look.icon != nullptr && look_icon) lv_label_set_display_text(look_icon, look.icon);
      if (look.has_color && ctx->btn) apply_button_colors(ctx->btn, true, look.color, true, look.color);
      break;
    }
  }
  if (ctx->show_icon_state && ctx->icon_lbl) {
    lv_label_set_display_text(ctx->icon_lbl, (!unavailable && is_entity_on_ref(state)) ? ctx->icon_on : ctx->icon_off);
    return;
  }
  if (ctx->show_text_state && ctx->status_text_lbl) {
    lv_label_set_display_text(ctx->status_text_lbl, text_sensor_display_text(state, HA_STATE_TEXT_MAX_LEN).c_str());
    return;
  }
  if (ctx->show_text_state && ctx->text_lbl) {
    set_wrapped_button_label_text(ctx->text_lbl, text_sensor_display_text(state, HA_STATE_TEXT_MAX_LEN));
    return;
  }
  if (!ctx->show_numeric_state || !ctx->sensor_lbl) return;

  float val = 0.0f;
  if (!unavailable && parse_float_ref(state, val) && std::isfinite(val)) {
    char buf[16];
    format_fixed_decimal(buf, sizeof(buf), val, ctx->precision);
    lv_label_set_display_text(ctx->sensor_lbl, buf);
    if (ctx->unit_lbl) lv_label_set_display_text(ctx->unit_lbl, ctx->unit.c_str());
  } else {
    lv_label_set_display_text(ctx->sensor_lbl, "");
    if (ctx->unit_lbl) lv_label_set_display_text(ctx->unit_lbl, "");
  }
}

inline void subscribe_action_card_display_state(ActionCardStateCtx *ctx,
                                                const std::string &entity_id) {
  if (!ctx || entity_id.empty()) return;
  if (!ctx->info_entity.empty()) {
    const std::string info_entity = ctx->info_entity;
    ha_subscribe_state(
      info_entity,
      std::function<void(esphome::StringRef)>([ctx, info_entity](esphome::StringRef state) {
        apply_action_card_info_value(ctx, state, ha_entity_state_unavailable_ref(info_entity, state));
      })
    );
  }
  ha_subscribe_state(
    entity_id,
    std::function<void(esphome::StringRef)>([ctx, entity_id](esphome::StringRef state) {
      bool unavailable = ha_entity_state_unavailable_ref(entity_id, state);
      bool active = !unavailable && (ctx->show_numeric_state
        ? numeric_state_positive_ref(state)
        : is_entity_on_ref(state));
      set_card_checked_state(ctx->btn, active);
      apply_action_card_display_value(ctx, state, unavailable);
    })
  );
}
