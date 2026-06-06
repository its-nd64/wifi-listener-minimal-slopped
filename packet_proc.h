#pragma once
#include "Arduino.h"
#include "config.h"
#include "esp_wifi.h"
#include "types.h"
#include "mac_utils.h"

QueueHandle_t pktQueue;

void removeWPA3RSNTag(uint8_t *buf, unsigned int *len) {
	if (*len < 36) return;
	unsigned int taggedParamsEnd = 36;

	while (taggedParamsEnd + 15 < *len) {
		uint8_t elementId = buf[taggedParamsEnd];
		uint8_t elementLength = buf[taggedParamsEnd + 1];

		if (elementId == 0x30) {
			unsigned int rsnStart = taggedParamsEnd;
			unsigned int rsnEnd = taggedParamsEnd + elementLength + 2;

			uint16_t akmCount = buf[taggedParamsEnd + 14];
			uint16_t akmPos = taggedParamsEnd + 16;

			for (int i = 0; i < akmCount; i++)
				if (buf[akmPos] == 0x00 && buf[akmPos + 1] == 0x0F && buf[akmPos + 2] == 0xAC && buf[akmPos + 3] == 0x08) {
					memmove(buf + akmPos, buf + akmPos + 4, *len - (akmPos + 4));
					*len -= 4;
					elementLength -= 4;
					buf[taggedParamsEnd + 1] = elementLength;
					akmCount--;
					buf[taggedParamsEnd + 14] = akmCount;
				} else akmPos += 4;

			if (akmCount == 0) {
				memmove(buf + rsnStart, buf + rsnEnd, *len - rsnEnd);
				*len -= (rsnEnd - rsnStart);
			}
		}

		taggedParamsEnd += 2 + elementLength;
	}
}

void insertCSA(uint8_t **buf, unsigned int *len, uint8_t newChannel, uint8_t switchCount) {
	if (*len < 36) return;
	*buf = (uint8_t*)realloc(*buf, *len + 11);
	unsigned int pos = 36;
	int insertPos = -1;

	while (pos + 2 <= *len) {
		uint8_t id = (*buf)[pos];
		uint8_t len_ie = (*buf)[pos + 1];

		if (pos + 2 + len_ie > *len) break;
		if (id == 0x03) insertPos = pos + 2 + len_ie;

		pos += 2 + len_ie;
	}
	if (insertPos == -1) insertPos = *len;

	uint8_t csaTag[] = { 
		0x25, 0x03, 0x01, newChannel, switchCount,		// CSA
		0x3C, 0x04, 0x01, 0x51, newChannel, switchCount // ECSA
	};
	memmove(*buf + insertPos + 11, *buf + insertPos, *len - insertPos);
	memcpy(*buf + insertPos, csaTag, 11);
	*len += 11;
}

void addSTA(AP_Info &ap, Mac mac, int rssi) {
	STA_Info sta;
	sta.mac = mac;
	sta.packetCounts.packetCount++;
	sta.packetCounts.packetsToAP = 1; // TODO: implement proper direction detection
	sta.rssiTmp = rssi;
	ap.STAs.push_back(sta);
	log1(COLOR_STA_ADDED, "+ %s AP: %s", BSSID2NAME(mac), ap.ssid);
}

const char* parseEapol(uint8_t *frame) {
	Mac macFrom, bssid;
	memcpy(macFrom.data(), frame + 10, 6);
	memcpy(bssid.data(), frame + 16, 6);

	if (macFrom == bssid) {
		return memcmp("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", frame + 115, 16) ? "M3" : "M1"; // mic all 0 = M1
	} else {
		return memcmp("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", frame + 51, 16) ? "M2" : "M4";  // nonce all 0 = M4
	}
}

void IRAM_ATTR addPkt(wifi_promiscuous_pkt_t *pkt) {
	int totalSize = sizeof(wifi_pkt_rx_ctrl_t) + pkt->rx_ctrl.sig_len;
	wifi_promiscuous_pkt_t* packet = (wifi_promiscuous_pkt_t*)malloc(totalSize);
	assert(packet);
	memcpy(&packet->rx_ctrl, &pkt->rx_ctrl, sizeof(wifi_pkt_rx_ctrl_t));
	memcpy(packet->payload, pkt->payload, pkt->rx_ctrl.sig_len);

	if (!xQueueSendFromISR(pktQueue, &packet, NULL)) {
		packetCounts::droppedPackets++;
		free(packet);
	}
}

void handle_packet(wifi_promiscuous_pkt_t *pkt) {
	if (pkt->rx_ctrl.rx_state != 0) return;

	uint8_t pktType    = pkt->payload[0] & 0x0C;
	uint8_t pktSubtype = pkt->payload[0] & 0xF0;
	uint16_t pktlen    = pkt->rx_ctrl.sig_len - 4;
	int rssi           = pkt->rx_ctrl.rssi;

	Mac macTo, macFrom, bssid;
	memcpy(macTo.data(),   pkt->payload + 4,  6);
	memcpy(macFrom.data(), pkt->payload + 10, 6);
	memcpy(bssid.data(),   pkt->payload + 16, 6);

	packetCounts::packetCount++;
	packetCounts::rssiTmp += rssi;

	addPktPcap(pktlen, pkt->payload);

	for (auto& ap : APs) if (memcmp(ap.bssid.data(), macFrom.data(), 6) == 0) {
		ap.packetCounts.packetCount++;
		ap.rssiTmp += rssi;
		break;
	}

	if (pktType == 0x04) return; // don't parse ctrl

	// resolve names only when needed (linear search through APs)
	if (pktType == 0x00) { // management
		const char* macToName   = BSSID2NAME(macTo);
		const char* macFromName = BSSID2NAME(macFrom);
		const char* bssidName   = BSSID2NAME(bssid);

		if (pktSubtype == 0x40) {
			packetCounts::probeCount++;
			if (isBroadcast(macTo)) log1(COLOR_PROBE_REQ, "Probe  Broadcast: %s", formatMac(macFrom));
			else log1(COLOR_PROBE_REQ, "Probe: %s -> %s", macFromName, macToName);
		}
		else if (pktSubtype == 0x50) {
			packetCounts::probeCount++;
			log1(COLOR_PROBE_RES, "Probe: %s -> %s", macFromName, macToName);
		}
		else if (pktSubtype == 0xB0 || pktSubtype == 0x00 || pktSubtype == 0x10) { // auth, assoc
			for (auto& ap: APs) {
				auto& staList = ap.STAs;
				for (auto it = staList.begin(); it != staList.end(); ++it) {
					if (it->mac == getStaMac(macTo, macFrom, bssid)) {
						log1(COLOR_STA_CONNECTING, "~ %s AP: %s", BSSID2NAME(it->mac), bssidName);
						staList.erase(it);
						return;
					}
				}
			}
			if (pktSubtype == 0xB0) log1(COLOR_AUTH, "%s Auth: %s -> %s", (pkt->payload[24] == 0x03 && pkt->payload[25] == 0x00) ? "S" : "W", macFromName, macToName);
			else log1(COLOR_ASSOC, "Assoc: %s -> %s", macFromName, macToName);
			return;
		}
		else if (pktSubtype == 0x20 || pktSubtype == 0x30) { // reassoc
			for (auto& ap: APs) {
				auto& staList = ap.STAs;
				for (auto it = staList.begin(); it != staList.end(); ++it) {
					if (it->mac == getStaMac(macTo, macFrom, bssid)) {
						log1(COLOR_STA_RECONNECTING, "R~ %s AP: %s", BSSID2NAME(it->mac), bssidName);
						staList.erase(it);
						return;
					}
				}
			}
			log1(COLOR_REASSOC, "Reassoc: %s -> %s", macFromName, macToName);
			return;
		}
		else if (pktSubtype == 0xC0 || pktSubtype == 0xA0) { // deauth, dissoc
			packetCounts::deauthCount++;
			if (isBroadcast(macTo)) log1(COLOR_DEAUTH, "%s Broadcast: %s", (pktSubtype == 0xC0) ? "Deauth" : "Dissoc", macFromName);
			else log1(COLOR_DEAUTH, "%s: %s -> %s", (pktSubtype == 0xC0) ? "Deauth" : "Dissoc", macFromName, macToName);
		}
		else if (pktSubtype == 0x80) { // beacon 👍
			for (auto& ap: APs) {
				if ((ap.wpa3Stuffs.hasWPA3 == 2 || ap.wpa3Stuffs.hasWPA3 == 3) && ap.bssid == bssid) {
					if (ap.wpa3Stuffs.CSAbeacon) { free(ap.wpa3Stuffs.CSAbeacon); ap.wpa3Stuffs.CSAbeacon = NULL; }

					if (ap.wpa3Stuffs.hasWPA3 == 2) {
						if (ap.wpa3Stuffs.WPA2beacon) { free(ap.wpa3Stuffs.WPA2beacon); ap.wpa3Stuffs.WPA2beacon = NULL; }
						ap.wpa3Stuffs.WPA2beaconLen = pktlen;
						ap.wpa3Stuffs.WPA2beacon = (uint8_t*)malloc(pktlen);
						memcpy(ap.wpa3Stuffs.WPA2beacon, pkt->payload, pktlen);
						removeWPA3RSNTag(ap.wpa3Stuffs.WPA2beacon, &ap.wpa3Stuffs.WPA2beaconLen);

						ap.wpa3Stuffs.CSAbeaconLen = ap.wpa3Stuffs.WPA2beaconLen;
						ap.wpa3Stuffs.CSAbeacon = (uint8_t*)malloc(ap.wpa3Stuffs.CSAbeaconLen);
						memcpy(ap.wpa3Stuffs.CSAbeacon, ap.wpa3Stuffs.WPA2beacon, ap.wpa3Stuffs.CSAbeaconLen);
					} else {
						ap.wpa3Stuffs.CSAbeaconLen = pktlen;
						ap.wpa3Stuffs.CSAbeacon = (uint8_t*)malloc(pktlen);
						memcpy(ap.wpa3Stuffs.CSAbeacon, pkt->payload, pktlen);
					}

					insertCSA(&ap.wpa3Stuffs.CSAbeacon, &ap.wpa3Stuffs.CSAbeaconLen, ((ap.channel + 3 > 13) ? 1 : ap.channel + 3), 3);
					ap.wpa3Stuffs.hasWPA3 = (ap.wpa3Stuffs.hasWPA3 == 2) ? 4 : 3;
				}
			}
			packetCounts::beaconCount++;
		}
	}

	// data frame
	if (pktType != 0x08 || pktlen < 28) return;

	if ((pkt->payload[30] == 0x88 && pkt->payload[31] == 0x8E) || (pkt->payload[32] == 0x88 && pkt->payload[33] == 0x8E)) { // eapol
		packetCounts::eapolCount++;
		const char* macToName   = BSSID2NAME(macTo);
		const char* macFromName = BSSID2NAME(macFrom);
		const char* text = parseEapol(pkt->payload);
		for (auto& ap: APs) if (ap.bssid == macFrom || ap.bssid == macTo) {
			log1(COLOR_EAPOL, "%s: %s -> %s", text, macToName, macFromName);
			log2(COLOR_EAPOL, "%s: %s -> %s", text, macToName, macFromName);
		}
		return;
	}

	packetCounts::dataCount++;
	if (macInvalid(macTo) || macInvalid(macFrom)) return;

	// add STA
	for (auto& ap: APs) {
		if (bssid == ap.bssid && (ap.bssid == macTo || ap.bssid == macFrom)) {
			Mac targetMac = (ap.bssid == macTo) ? macFrom : macTo;
			for (auto& sta: ap.STAs) if (targetMac == sta.mac) {
				sta.packetCounts.packetCount++;
				(ap.bssid == macTo ? sta.packetCounts.packetsFromAP : sta.packetCounts.packetsToAP)++;
				sta.rssiTmp += rssi;
				return;
			}
			addSTA(ap, targetMac, rssi);
		}
	}
}

void init_packet_processor() {
	pktQueue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(wifi_promiscuous_pkt_t*));

	xTaskCreatePinnedToCore([](void* _) {
		wifi_promiscuous_pkt_t* packet;
		while (1) {
			while (xQueueReceive(pktQueue, &packet, 0) == pdTRUE) {
				handle_packet(packet);
				free(packet);
			}
			delay(1);
		}
	}, "pkt proc", PACKET_PROCESSOR_STACK_SIZE, NULL, 2, NULL, PACKET_PROCESSOR_CORE);
}
