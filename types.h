#pragma once
#include "Arduino.h"
#include "config.h"
#include "vector"

using namespace std;
using Mac = array<uint8_t, 6>;

namespace packetCounts {
	unsigned long packetCount, beaconCount, deauthCount, probeCount, dataCount, eapolCount;
	int rssiTmp, rssi;
	unsigned long droppedPackets;
}

struct STA_Info {
	Mac mac;
	int rssiTmp = 0, rssi = 0;

	struct packetCounts {
		int packetCount = 0;
		int packetsFromAP = 0;
		int packetsToAP = 0;
		unsigned long lastPacketUpdate = 0;
	} packetCounts;
};

struct AP_Info {
	char ssid[AP_SSID_MAX_LEN + 1] = {0};
	Mac bssid;
	uint8_t channel = 0;
	vector<STA_Info> STAs;
	int rssiTmp = 0, rssi = 0;

	struct wpa3Stuffs {
		// -1: open, 0: wpa2, 1: wpa3 only, 2: wpa2+3
		// 3, 4: done processing beacons
		int hasWPA3 = 0;
		uint8_t* CSAbeacon = NULL;
		size_t CSAbeaconLen = 0;
		uint8_t* WPA2beacon = NULL;
		size_t WPA2beaconLen = 0;
	} wpa3Stuffs;

	struct packetCounts {
		int packetCount = 0;
		unsigned long lastPacketUpdate = 0;
	} packetCounts;
};

vector<AP_Info> APs;
vector<uint8_t> channels;
size_t channelIndex = 0;
bool deauthActive;

bool disableSD;
bool alwaysSendWPA2Beacon;
bool debugMenuEnabled;