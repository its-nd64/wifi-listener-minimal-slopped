#pragma once
#include "Arduino.h"
#include "config.h"
#include "log.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

extern TFT_eSprite sprite;

struct SDpkt {
	uint32_t tS;
	uint32_t tmS;
	uint16_t len;
	uint8_t* payload;
};

bool haveSD;
QueueHandle_t SDpktQueue;
SemaphoreHandle_t sdInitLock;
SemaphoreHandle_t sdWriteLock;

FILE* fil;
uint8_t writeBuf[SD_WRITE_BUF_SIZE];
uint16_t writeBufPos = 0;

void flushWriteBuf() {
	if (!writeBufPos) return;
	size_t written = fwrite(writeBuf, 1, writeBufPos, fil);
	if (written != writeBufPos) log1(COLOR_NO_SD, "SD write err: %d/%d", written, writeBufPos);
	writeBufPos = 0;
}

void flushWriteBufSafe() {
	if (xSemaphoreTake(sdWriteLock, portMAX_DELAY)) {
		flushWriteBuf();
		fflush(fil);
		fsync(fileno(fil));
		xSemaphoreGive(sdWriteLock);
	}
}

void bufWrite(const uint8_t* data, uint16_t len) {
	uint16_t pos = 0;
	while (pos < len) {
		uint16_t chunk = min((uint16_t)(SD_WRITE_BUF_SIZE - writeBufPos), (uint16_t)(len - pos));
		memcpy(writeBuf + writeBufPos, data + pos, chunk);
		writeBufPos += chunk;
		pos += chunk;
		if (writeBufPos == SD_WRITE_BUF_SIZE) flushWriteBuf();
	}
}

void writePcapPacket(uint32_t tS, uint32_t tmS, uint16_t len, const uint8_t* payload) {
	uint8_t hdr[16];
	uint32_t caplen = len, origlen = len;
	memcpy(hdr,  &tS, 4);
	memcpy(hdr + 4, &tmS, 4);
	memcpy(hdr + 8, &caplen, 4);
	memcpy(hdr + 12, &origlen,4);
	bufWrite(hdr, 16);
	bufWrite(payload, len);
}

void init_sd() {
	sdInitLock  = xSemaphoreCreateBinary();
	sdWriteLock = xSemaphoreCreateMutex();
	xSemaphoreGive(sdInitLock);
	xSemaphoreTake(sdInitLock, portMAX_DELAY);

	spi_bus_config_t bus = {
		.mosi_io_num = SD_PIN_MOSI,
		.miso_io_num = SD_PIN_MISO,
		.sclk_io_num = SD_PIN_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = SD_WRITE_BUF_SIZE,
	};

	sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot.gpio_cs = (gpio_num_t)SD_PIN_CS;
	slot.host_id = SPI2_HOST;

	sdmmc_host_t host = SDSPI_HOST_DEFAULT();

	esp_vfs_fat_sdmmc_mount_config_t mount = {
		.format_if_mount_failed = false,
		.max_files = 4,
		.allocation_unit_size = 16 * 1024,
	};

	sdmmc_card_t* card = nullptr;

	static const int freqs[] = { 400, 1000, 5000, 10000, 20000 };
	bool mounted = false;
	for (int freq : freqs) {
		host.max_freq_khz = freq;
		spi_bus_initialize(SPI2_HOST, &bus, SDSPI_DEFAULT_DMA);
		if (esp_vfs_fat_sdspi_mount("/sd", &host, &slot, &mount, &card) == ESP_OK) {
			mounted = true;
			break;
		}
		if (card) { esp_vfs_fat_sdcard_unmount("/sd", card); card = nullptr; }
		spi_bus_free(SPI2_HOST);
	}

	if (!mounted) {
		log1(COLOR_NO_SD, "No SD Card!");
		xSemaphoreGive(sdInitLock);
		return;
	}

	haveSD = true;
	uint64_t cardSize = (uint64_t)card->csd.capacity * card->csd.sector_size / (1024 * 1024);
	log1(COLOR_FOUND_SD, "SD Card Found, Size: %lluMB @ %dkHz", cardSize, host.max_freq_khz);
	sprite.printf("SD Card Found, Size: %lluMB @ %dkHz\n", cardSize, host.max_freq_khz);

	int maxNum = 0;
	DIR* dir = opendir("/sd");
	if (dir) {
		struct dirent* ent;
		while ((ent = readdir(dir)) != NULL) {
			const char* name = ent->d_name;
			int len = strlen(name);
			if (len > 5 && strcmp(name + len - 5, ".pcap") == 0) {
				int num = atoi(name);
				if (num > maxNum) {
					char checkpath[32];
					snprintf(checkpath, sizeof(checkpath), "/sd/%s", name);
					struct stat st;
					if (stat(checkpath, &st) == 0 && st.st_size >= 25) maxNum = num;
				}
			}
		}
		closedir(dir);
	}

	char filepath[32];
	snprintf(filepath, sizeof(filepath), "/sd/%d.pcap", maxNum + 1);
	fil = fopen(filepath, "wb");
	if (!fil) {
		log1(COLOR_NO_SD, "File open failed! errno:%d", errno);
		xSemaphoreGive(sdInitLock);
		return;
	}

	static const uint8_t pcapHdr[24] = {
		0xD4, 0xC3, 0xB2, 0xA1,
		0x02, 0x00,
		0x04, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0xFF, 0xFF, 0x00, 0x00,
		0x69, 0x00, 0x00, 0x00,
	};
	fwrite(pcapHdr, 1, 24, fil);
	fflush(fil);
	fsync(fileno(fil));

	log1(COLOR_PCAP_FILE_OPENED, "File: %s", filepath);
	sprite.printf("File: %s\n", filepath);

	SDpktQueue = xQueueCreate(SD_QUEUE_SIZE, sizeof(SDpkt));

	xTaskCreatePinnedToCore([](void* _) {
		SDpkt packet;
		while (haveSD) {
			while (xQueueReceive(SDpktQueue, &packet, 0) == pdTRUE) {
				if (xSemaphoreTake(sdWriteLock, portMAX_DELAY)) {
					writePcapPacket(packet.tS, packet.tmS, packet.len, packet.payload);
					xSemaphoreGive(sdWriteLock);
				}
				free(packet.payload);
			}
			delay(3);
		}
		vTaskDelete(NULL);
	}, "sd pkts", SD_TASK_STACK_SIZE, NULL, 3, NULL, SD_TASK_CORE);

	xTaskCreatePinnedToCore([](void* _) {
		while (1) {
			if (haveSD) flushWriteBufSafe();
			else vTaskDelete(NULL);
			delay(FLUSH_INTERVAL);
		}
	}, "flush", FLUSH_TASK_STACK_SIZE, NULL, 2, NULL, FLUSH_TASK_CORE);

	xSemaphoreGive(sdInitLock);
}

IRAM_ATTR void addPktPcap(uint16_t len, uint8_t* payload) {
	if (haveSD) {
		SDpkt packet = {
			.tS  = millis() / 1000,
			.tmS = micros() - millis() * 1000,
			.len = len,
			.payload = (uint8_t*)ps_malloc(len)
		};
		if (packet.payload) {
			memcpy(packet.payload, payload, len);
			if (!xQueueSendFromISR(SDpktQueue, &packet, 0)) free(packet.payload);
		}
	}
}