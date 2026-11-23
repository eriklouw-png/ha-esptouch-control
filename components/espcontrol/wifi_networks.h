#pragma once

#include <string>
#include <vector>

#include "esphome/components/wifi/wifi_component.h"

// Several saved WiFi networks in connection order. The panel's web page stores up to five
// networks (SSID + password per slot) and the slot order; at boot the networks are added to
// the WiFi component with a priority that follows the order (first = highest). The network the
// panel already had saved stays as the last fallback.
constexpr int WIFI_NETWORK_SLOTS = 5;

// "3,1,2" -> {2, 0, 1} (zero-based slots). Slots missing from the text follow in numeric order.
inline std::vector<int> wifi_network_slot_order(const std::string &order) {
  std::vector<int> slots;
  size_t pos = 0;
  while (pos <= order.size()) {
    size_t end = order.find(',', pos);
    if (end == std::string::npos) end = order.size();
    const std::string token = order.substr(pos, end - pos);
    pos = end + 1;
    if (token.size() != 1 || token[0] < '1' || token[0] > '0' + WIFI_NETWORK_SLOTS) continue;
    const int slot = token[0] - '1';
    bool seen = false;
    for (int existing : slots) seen = seen || existing == slot;
    if (!seen) slots.push_back(slot);
  }
  for (int slot = 0; slot < WIFI_NETWORK_SLOTS; slot++) {
    bool seen = false;
    for (int existing : slots) seen = seen || existing == slot;
    if (!seen) slots.push_back(slot);
  }
  return slots;
}

// Priority of the network at position `index` of the order: 100, 80, 60, 40, 20 (the saved
// network that was already on the panel keeps 0 and is tried last).
inline int8_t wifi_network_priority(int index) {
  return static_cast<int8_t>(100 - index * 20);
}

inline void wifi_networks_apply(const std::string *ssids, const std::string *passwords, const std::string &order) {
  auto *wifi = esphome::wifi::global_wifi_component;
  if (wifi == nullptr) return;
  const std::vector<int> slots = wifi_network_slot_order(order);
  int added = 0;
  for (size_t index = 0; index < slots.size(); index++) {
    const int slot = slots[index];
    if (ssids[slot].empty()) continue;
    esphome::wifi::WiFiAP ap;
    ap.set_ssid(ssids[slot]);
    ap.set_password(passwords[slot]);
    ap.set_priority(wifi_network_priority(static_cast<int>(index)));
    wifi->add_sta(ap);
    added++;
  }
  ESP_LOGI("wifi_networks", "Added %d saved network(s) in connection order", added);
}
