#include "udp_nmea2000_config.h"

#include <Preferences.h>
#include <string.h>

namespace {
constexpr const char *kNamespace = "udp_n2k";
}

void udp_config_load(UdpNmea2000Config *config) {
  if (config == nullptr) return;

  memset(config, 0, sizeof(*config));
  config->enabled = N2K_UDP_DEFAULT_ENABLED;
  strncpy(config->wifi_ssid, N2K_UDP_DEFAULT_WIFI_SSID, sizeof(config->wifi_ssid) - 1);
  strncpy(config->wifi_password, N2K_UDP_DEFAULT_WIFI_PASSWORD, sizeof(config->wifi_password) - 1);
  config->local_port = N2K_UDP_DEFAULT_LOCAL_PORT;
  strncpy(config->remote_ip, N2K_UDP_DEFAULT_REMOTE_IP, sizeof(config->remote_ip) - 1);
  config->remote_port = N2K_UDP_DEFAULT_REMOTE_PORT;

  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return;
  config->enabled = preferences.getBool("enabled", config->enabled);
  preferences.getString("ssid", config->wifi_ssid, sizeof(config->wifi_ssid));
  preferences.getString("password", config->wifi_password, sizeof(config->wifi_password));
  config->local_port = preferences.getUShort("local_port", config->local_port);
  preferences.getString("remote_ip", config->remote_ip, sizeof(config->remote_ip));
  config->remote_port = preferences.getUShort("remote_port", config->remote_port);
  preferences.end();
}

bool udp_config_save(const UdpNmea2000Config *config) {
  if (config == nullptr) return false;

  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  preferences.putBool("enabled", config->enabled);
  preferences.putString("ssid", config->wifi_ssid);
  preferences.putString("password", config->wifi_password);
  preferences.putUShort("local_port", config->local_port);
  preferences.putString("remote_ip", config->remote_ip);
  preferences.putUShort("remote_port", config->remote_port);
  preferences.end();
  return true;
}