#include "udp_config_screen.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#include <string.h>

#include "udp_nmea2000_config.h"
#include "ui.h"

namespace {
lv_obj_t *screen = nullptr;
lv_obj_t *keyboard = nullptr;
lv_obj_t *udp_enabled = nullptr;
lv_obj_t *ssid_field = nullptr;
lv_obj_t *password_field = nullptr;
lv_obj_t *ip_field = nullptr;
lv_obj_t *local_port_field = nullptr;
lv_obj_t *remote_port_field = nullptr;
lv_obj_t *status_label = nullptr;

void ensure_keyboard();
void create_screen();

void set_field(lv_obj_t *field, const char *value) {
  lv_textarea_set_text(field, value != nullptr ? value : "");
}

void keyboard_event(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(lv_event_get_target(event), LV_STATE_FOCUSED);
  }
}

void field_event(lv_event_t *event) {
  const lv_event_code_t code = lv_event_get_code(event);
  Serial.printf("UDP config: field event code=%d target=%p\n", static_cast<int>(code), lv_event_get_target(event));
  if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;
  ensure_keyboard();
  lv_obj_t *field = lv_event_get_target(event);
  Serial.printf("UDP config: field target=%p keyboard=%p\n", field, keyboard);
  if (keyboard == nullptr) return;
  lv_obj_add_state(field, LV_STATE_FOCUSED);
  lv_keyboard_set_textarea(keyboard, field);
  lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  if (field == ip_field || field == local_port_field || field == remote_port_field) {
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
  } else {
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  }
  lv_obj_invalidate(keyboard);
  lv_refr_now(lv_disp_get_default());
  Serial.printf("UDP config: keyboard hidden=%u textarea=%p\n",
                lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) ? 1U : 0U,
                lv_keyboard_get_textarea(keyboard));
}

lv_obj_t *make_field(const char *label, lv_coord_t y, bool password = false) {
  lv_obj_t *caption = lv_label_create(screen);
  lv_label_set_text(caption, label);
  lv_obj_set_style_text_color(caption, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_label_set_text(caption, label);
  lv_obj_set_pos(caption, 18, y + 8);

  lv_obj_t *field = lv_textarea_create(screen);
  lv_obj_set_size(field, 292, 40);
  lv_obj_set_pos(field, 160, y);
  lv_textarea_set_one_line(field, true);
  lv_textarea_set_password_mode(field, password);
  lv_obj_add_event_cb(field, field_event, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(field, field_event, LV_EVENT_CLICKED, nullptr);
  return field;
}

void ensure_keyboard() {
  if (keyboard != nullptr) {
    Serial.printf("UDP config: keyboard already exists %p hidden=%u\n",
                  keyboard, lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) ? 1U : 0U);
    return;
  }

  Serial.printf("UDP config: creating keyboard on top layer, screen=%p\n", screen);
  keyboard = lv_keyboard_create(lv_layer_top());
  Serial.printf("UDP config: keyboard created %p\n", keyboard);
  if (keyboard == nullptr) return;
  lv_obj_set_size(keyboard, 480, 150);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(keyboard, lv_color_hex(0x000000), LV_PART_ITEMS);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_CANCEL, nullptr);
}

void close_screen(lv_event_t *) {
  if (keyboard != nullptr) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  if (screen != nullptr) lv_scr_load(ui_ScrInfo);
}

void save_screen(lv_event_t *) {
  UdpNmea2000Config config;
  udp_config_load(&config);
  config.enabled = lv_obj_has_state(udp_enabled, LV_STATE_CHECKED);
  strncpy(config.wifi_ssid, lv_textarea_get_text(ssid_field), sizeof(config.wifi_ssid) - 1);
  strncpy(config.wifi_password, lv_textarea_get_text(password_field), sizeof(config.wifi_password) - 1);
  strncpy(config.remote_ip, lv_textarea_get_text(ip_field), sizeof(config.remote_ip) - 1);
  config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
  config.remote_ip[sizeof(config.remote_ip) - 1] = '\0';
  config.local_port = static_cast<uint16_t>(strtoul(lv_textarea_get_text(local_port_field), nullptr, 10));
  config.remote_port = static_cast<uint16_t>(strtoul(lv_textarea_get_text(remote_port_field), nullptr, 10));

  IPAddress parsed_ip;
  if (config.local_port == 0 || config.remote_port > 65535 ||
      !parsed_ip.fromString(config.remote_ip) || config.wifi_ssid[0] == '\0') {
    lv_label_set_text(status_label, "Enter a valid IP and local port");
    return;
  }
  if (!udp_config_save(&config)) {
    lv_label_set_text(status_label, "Could not save settings");
    return;
  }

  lv_label_set_text(status_label, "Saved. Restarting...");
  lv_refr_now(nullptr);
  delay(250);
  esp_restart();
}

void open_screen(lv_event_t *) {
  Serial.println("UDP config: Network button event");
  create_screen();
  Serial.printf("UDP config: screen=%p ssid=%p\n", screen, ssid_field);
  if (screen == nullptr || ssid_field == nullptr) {
    Serial.println("UDP config: screen allocation failed");
    return;
  }
  UdpNmea2000Config config;
  udp_config_load(&config);
  lv_obj_add_state(udp_enabled, LV_STATE_CHECKED);
  if (!config.enabled) lv_obj_clear_state(udp_enabled, LV_STATE_CHECKED);
  set_field(ssid_field, config.wifi_ssid);
  set_field(password_field, config.wifi_password);
  set_field(ip_field, config.remote_ip);
  char value[8];
  snprintf(value, sizeof(value), "%u", config.local_port);
  set_field(local_port_field, value);
  snprintf(value, sizeof(value), "%u", config.remote_port);
  set_field(remote_port_field, value);
  lv_label_set_text(status_label, "");
  lv_scr_load(screen);
  Serial.printf("UDP config: screen loaded active=%p\n", lv_scr_act());

  ensure_keyboard();
  if (keyboard == nullptr) {
    Serial.println("UDP config: keyboard allocation failed");
    return;
  }
  lv_keyboard_set_textarea(keyboard, ssid_field);
  lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(keyboard);
  lv_obj_invalidate(screen);
  lv_obj_invalidate(keyboard);
  lv_refr_now(lv_disp_get_default());
  lv_obj_t *top_layer = lv_layer_top();
  Serial.printf("UDP config: keyboard shown hidden=%u textarea=%p geom=%d,%d %dx%d parent=%p top=%p top_hidden=%u\n",
                lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) ? 1U : 0U,
                lv_keyboard_get_textarea(keyboard),
                static_cast<int>(lv_obj_get_x(keyboard)),
                static_cast<int>(lv_obj_get_y(keyboard)),
                static_cast<int>(lv_obj_get_width(keyboard)),
                static_cast<int>(lv_obj_get_height(keyboard)),
                lv_obj_get_parent(keyboard),
                top_layer,
                lv_obj_has_flag(top_layer, LV_OBJ_FLAG_HIDDEN) ? 1U : 0U);
}

void create_screen() {
  if (screen != nullptr) return;
  screen = lv_obj_create(nullptr);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x20252A), LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "UDP Actisense");
  lv_obj_set_pos(title, 18, 8);

  udp_enabled = lv_checkbox_create(screen);
  lv_checkbox_set_text(udp_enabled, "Enable UDP");
  lv_obj_set_pos(udp_enabled, 300, 8);
  lv_obj_set_style_text_color(udp_enabled, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(udp_enabled, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);

  ssid_field = make_field("Wi-Fi name", 52);
  password_field = make_field("Password", 98, true);
  ip_field = make_field("Source IP", 144);
  local_port_field = make_field("Local port", 190);
  remote_port_field = make_field("Source port", 236);

  status_label = lv_label_create(screen);
  lv_obj_set_pos(status_label, 18, 286);

  lv_obj_t *back = lv_btn_create(screen);
  lv_obj_set_size(back, 100, 48);
  lv_obj_set_pos(back, 18, 280);
  lv_obj_add_event_cb(back, close_screen, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *back_label = lv_label_create(back);
  lv_label_set_text(back_label, "Back");
  lv_obj_center(back_label);

  lv_obj_t *save = lv_btn_create(screen);
  lv_obj_set_size(save, 120, 48);
  lv_obj_set_pos(save, 330, 280);
  lv_obj_add_event_cb(save, save_screen, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *save_label = lv_label_create(save);
  lv_label_set_text(save_label, "Save");
  lv_obj_center(save_label);

}

} // namespace

void udp_config_screen_attach(lv_obj_t *settings_screen) {
  Serial.printf("UDP config: attaching Network button to %p\n", settings_screen);
  lv_obj_t *button = lv_btn_create(settings_screen);
  lv_obj_set_size(button, 170, 55);
  lv_obj_set_pos(button, 155, 409);
  lv_obj_add_event_cb(button, open_screen, LV_EVENT_RELEASED, nullptr);
  lv_obj_t *button_label = lv_label_create(button);
  lv_label_set_text(button_label, "Network");
  lv_obj_center(button_label);
  Serial.printf("UDP config: Network button created %p\n", button);
}