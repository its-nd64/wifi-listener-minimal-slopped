#pragma once

// logs
#define MAX_LOG_LEN 64
#define LOG_QUEUE_SIZE 64
#define LOG_RING_SIZE 30
#define LOG_TASK_STACK_SIZE 2000
#define LOG_TASK_CORE 1

// wifi
#define AP_SSID_MAX_LEN 18
#define SCAN_TIME_MIN 150
#define SCAN_TIME_MAX 200
#define FILTER_MASK WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
#define FILTER_MASK_CTRL 0
#define COLOR_SD_INIT_NOT_FINISHED 0xF800
#define COLOR_SD_INIT_FINISHED 0x07E0
#define PACKET_QUEUE_SIZE 1000
#define PACKET_PROCESSOR_STACK_SIZE 5000
#define PACKET_PROCESSOR_CORE 1
// #define ALWAYS_SEND_WPA2_BEACON true // significantly increase sta capture chance and allow capturing of WPA2 handshakes even when
//                                      // deauth is not active, making handshake capture much more reliable
//                                      // BUT, it also significantly increases power consumption and makes the listener non-stealthy
                                        // forgot that i can js add hardware switch, moved

// packet
#define COLOR_PROBE_REQ 0x07FF
#define COLOR_PROBE_RES 0x867D
#define COLOR_AUTH 0xFDA0
#define COLOR_ASSOC COLOR_AUTH
#define COLOR_REASSOC COLOR_AUTH
#define COLOR_DEAUTH 0xF883
#define COLOR_EAPOL 0xFEA0
// sta
#define COLOR_STA_CONNECTING 0x35A6
#define COLOR_STA_RECONNECTING 0x35A6
#define COLOR_STA_ADDED 0xB7E0

// sd
#define SD_PIN_MOSI 23
#define SD_PIN_MISO 19
#define SD_PIN_CLK 18
#define SD_PIN_CS 5
#define SD_WRITE_BUF_SIZE 4096
#define SD_QUEUE_SIZE 64
#define SD_TASK_STACK_SIZE 4000
#define SD_TASK_CORE 1
#define COLOR_FOUND_SD 0x07E0
#define COLOR_NO_SD 0xF800
#define COLOR_PCAP_FILE_OPENED 0xFFFF
#define COLOR_SD_FREQ 0xFFFF
#define FLUSH_INTERVAL 1000
#define FLUSH_TASK_STACK_SIZE 4000
#define FLUSH_TASK_CORE 1

// display
#define MAIN_MENU_UPDATE_INTERVAL 1000
#define LOG_MENU_UPDATE_INTERVAL 100
#define COLOR_AP_BEACON_NOT_CAPTURED 0xF800
#define COLOR_AP_WPA23_BEACON_CAPTURED 0x87E0
#define COLOR_AP_WPA3_BEACON_CAPTURED 0xFFE0
#define COLOR_AP_WPA2 0xCF7B
#define COLOR_AP_OPEN 0x07FF

// main
#define UPDATE_TASK_STACK_SIZE 3000
#define UPDATE_TASK_CORE 1
#define BUTTON_TASK_STACK_SIZE 4000
#define BUTTON_TASK_CORE 1
#define BUTTON_TICK_INTERVAL 10
#define UPDATE_TASK_INTERVAL 1000
#define DEAUTH_INTERVAL 100
#define STA_AP_UPDATE_TASK_STACK_SIZE 3000
#define STA_AP_UPDATE_TASK_CORE 1
#define DEAUTH_TASK_STACK_SIZE 3000
#define DEAUTH_TASK_CORE 0
#define DETECTED_DROPPED_PACKETS_COLOR 0xF883

// mon thing
#define MONITOR_TASK_STACK_SIZE 2000
#define MONITOR_TASK_CORE 1
#define STAT_BAR_COLOR 0xFEA0

// switches
#define DISABLE_SD_PIN 32
#define ALWAYS_SEND_WPA2_BEACON_PIN 33
#define ENABLE_DEBUG_MENU_PIN 25
