#pragma once

#include <stdbool.h>
#include <stdint.h>

#define N2K_UDP_DEFAULT_WIFI_SSID "CHANGE_ME"
#define N2K_UDP_DEFAULT_WIFI_PASSWORD "CHANGE_ME"
#define N2K_UDP_DEFAULT_LOCAL_PORT 10120
#define N2K_UDP_DEFAULT_REMOTE_IP "192.168.1.140"
#define N2K_UDP_DEFAULT_REMOTE_PORT 0
#define N2K_UDP_DEFAULT_ENABLED true

#define N2K_UDP_WIFI_CONNECT_TIMEOUT_MS 15000
#define N2K_UDP_STREAM_BUFFER_SIZE 4096

typedef struct {
	bool enabled;
	char wifi_ssid[64];
	char wifi_password[64];
	uint16_t local_port;
	char remote_ip[16];
	uint16_t remote_port;
} UdpNmea2000Config;

void udp_config_load(UdpNmea2000Config *config);
bool udp_config_save(const UdpNmea2000Config *config);