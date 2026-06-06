#pragma once
#include "Arduino.h"
#include "types.h"
#include "log.h"
#include "esp_wifi.h"
#include "config.h"

inline bool isBroadcast(const Mac mac) {
	const Mac broadcastMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	return mac == broadcastMac;
}

inline bool macInvalid(const Mac mac) {
	const Mac zeroMac = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return mac == zeroMac || isBroadcast(mac) || mac[0] & 0x01;
}

// @warning ONLY 1 CALL PER STATEMENT!!!
const char* formatMac(Mac mac) {
	thread_local char result[18];
	snprintf(result, sizeof(result), MACSTR, MAC2STR(mac));
	for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);
	return result;
}

// @warning ONLY 1 CALL PER STATEMENT!!!
const char* BSSID2NAME(const Mac& mac) {
	thread_local char buffer1[128];
	auto it = find_if(APs.begin(), APs.end(), [&](const AP_Info& ap){ return ap.bssid == mac; });
	if (it != APs.end()) {
		size_t len = strlen(it->ssid);
		if (len >= sizeof(buffer1)) len = sizeof(buffer1) - 1;
		memcpy(buffer1, it->ssid, len);
		buffer1[len] = 0;
		return buffer1;
	}
	return formatMac(mac);
}

Mac getStaMac(const Mac macFrom, const Mac macTo, const Mac bssid) { // only mgmt
	if (macTo == bssid) return macFrom;
	else if (macFrom == bssid) return macTo;
	else if (isBroadcast(bssid) || isBroadcast(macTo)) return macFrom;
	else return {0xBA, 0xAD, 0xF0, 0x0D, 0x12, 0x34};
}
