#pragma once
#include "map"
#include "vector"
#include "Arduino.h"
#include "TFT_eSPI.h"
#include "types.h"
#include "mon_thing.h"
#include "sd_stuffs.h"

using namespace std;

TFT_eSPI tft = TFT_eSPI(240, 320);
TFT_eSprite sprite = TFT_eSprite(&tft);

/* Menus
	0: Home menu
	1: Log1 menu
	2: Log2 menu(sub)
	3: FreeRTOS Tasks
*/
int menuIndex;
SemaphoreHandle_t drawSemaphore;

inline uint16_t colorizeRSSI(int rssi) {
	return rssi > -60 ? 0x2FE5 : rssi > -70 ? 0xFFE0 : rssi > -85 ? 0xFDA0 : 0xF800;
}

inline uint16_t colorizeAP(int hasWPA3) {
	if (hasWPA3 == 1 || hasWPA3 == 2) return COLOR_AP_BEACON_NOT_CAPTURED;
	else if (hasWPA3 == 3) return COLOR_AP_WPA3_BEACON_CAPTURED;
	else if (hasWPA3 == 4) return COLOR_AP_WPA23_BEACON_CAPTURED;
	else if (hasWPA3 == -1) return COLOR_AP_OPEN;
	else return COLOR_AP_WPA2;
}

static unsigned long startTime;
static bool firstPrint = true;
void printDebugWithTime(const char* name) {
	if (firstPrint) { // dihahh implimentation to make a global var static
		startTime = millis();
		firstPrint = false;
	}
	sprite.printf("%s in %lums\n\n", name, millis() - startTime);
	sprite.pushSprite(0, 0);
	startTime = millis();
}

void init_display() {
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
    tft.setTextWrap(false);
    sprite.setColorDepth(8); 
	sprite.createSprite(tft.width(), tft.height());
    sprite.setTextWrap(false);
	sprite.fillSprite(TFT_BLACK);

	drawSemaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(drawSemaphore);
}

void drawStatsbar() {
    sprite.setTextColor(STAT_BAR_COLOR);
	sprite.printf("CPU: %.1f/%.1f%% RAM: %.1f%%/%.1f%% %s%ds\n", cpuUsage0, cpuUsage1, (1.0f - (float)ESP.getFreeHeap() / ESP.getHeapSize()) * 100.0f, (ESP.getPsramSize() > 0 ? (1.0f - (float)ESP.getFreePsram() / ESP.getPsramSize()) * 100.0f : 0.0f), haveSD ? "SD " : "", (int)(millis() / 1000));
    // sprite.printf("%.1f/%.1f%% %u/%u/%u+%u/%u/%uKB %s%ds\n", cpuUsage0, cpuUsage1, ESP.getMaxAllocHeap() / 1000, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, ESP.getMaxAllocPsram() / 1000, ESP.getFreePsram() / 1000, ESP.getPsramSize() / 1000, haveSD ? "SD " : "", (int)millis() / 1000);
}

void display_draw() {
    if (xSemaphoreTake(drawSemaphore, 5000)) {
		sprite.fillSprite(TFT_BLACK);
		sprite.setCursor(0, 0);

		if (menuIndex == 0) {
			drawStatsbar();
			sprite.setTextColor(TFT_WHITE);
			sprite.printf("Ch:%-2i|", channels[channelIndex]);
			sprite.setTextColor(0xD69A);
			sprite.printf("Pkt:%-5i|", packetCounts::packetCount);
			sprite.setTextColor(0xA7FF);
			sprite.printf("Be:%-2i|", packetCounts::beaconCount);
			sprite.setTextColor(0xF410);
			sprite.printf("De:%-2i|", packetCounts::deauthCount);
			sprite.setTextColor(TFT_CYAN);
			sprite.printf("Pb:%-2i|", packetCounts::probeCount);
			sprite.setTextColor(TFT_GREENYELLOW);
			sprite.printf("Da:%-4i|", packetCounts::dataCount);
			sprite.setTextColor(TFT_YELLOW);
			sprite.printf("EP:%-3i|", packetCounts::eapolCount);
			sprite.setTextColor(colorizeRSSI(packetCounts::rssi));
			sprite.printf("%-2i\n", packetCounts::rssi);
			sprite.setTextColor(TFT_WHITE);
			for (auto &ap : APs ) if (ap.channel == channels[channelIndex]) {
				sprite.setTextColor(colorizeAP(ap.wpa3Stuffs.hasWPA3));
				sprite.printf("%s", ap.ssid);
				sprite.setTextColor(TFT_WHITE);
				sprite.printf("|%d STA|", ap.STAs.size());
				sprite.setTextColor(colorizeRSSI(ap.rssi));
				sprite.printf("%-2i\n", ap.rssi); 
				sprite.setTextColor(TFT_WHITE);
				for (auto &sta : ap.STAs) {
					sprite.printf(" %02X:%02X:%02X:%02X:%02X:%02X ",MAC2STR(sta.mac));
					sprite.setTextColor(colorizeRSSI(sta.rssi));
					sprite.printf("%-2i", sta.rssi);
					sprite.setTextColor(TFT_WHITE);
					sprite.printf(" %dT %dR\n", sta.packetCounts.packetsToAP, sta.packetCounts.packetsFromAP);
				}
			}
		} else if (menuIndex == 1) {
			for (int i = 0; i < logListSize1; i++) {
				int idx = (logListHead1 + i) % LOG_RING_SIZE;
				sprite.setTextColor(logList1[idx].color);
				sprite.println(logList1[idx].message);
			}
		} else if (menuIndex == 2) {
			for (int i = 0; i < logListSize2; i++) {
				int idx = (logListHead2 + i) % LOG_RING_SIZE;
				sprite.setTextColor(logList2[idx].color);
				sprite.println(logList2[idx].message);
			}
		} else if (menuIndex == 3) {
			xSemaphoreTake(monMutex, portMAX_DELAY);
			static vector<pair<pair<float, float>, TaskStatus_t>> taskCpuUsageList;
			static std::map<String, uint32_t> prevTaskRuntime;
			static uint32_t prevRuntime = 0;
			static uint32_t lastUpdateMs = 0;
			static int lastSortIndex = -1;

			uint32_t currentTime = millis();
			uint32_t elapsedTotalRunTime = totalRunTime - prevRuntime;

			if (currentTime - lastUpdateMs >= 1000 && taskStatusArray) {
				lastUpdateMs = currentTime;
				prevRuntime = totalRunTime;
				taskCpuUsageList.clear();

				const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
				for (UBaseType_t i = 0; i < taskCount; i++) {
					String taskName(taskStatusArray[i].pcTaskName);
					uint32_t prevTaskRT = prevTaskRuntime[taskName];
					uint32_t taskElapsedRuntime = taskStatusArray[i].ulRunTimeCounter - prevTaskRT;
					prevTaskRuntime[taskName] = taskStatusArray[i].ulRunTimeCounter;
					float taskCpuUsage = (elapsedTotalRunTime > 0) ? ((float)taskElapsedRuntime / (float)elapsedTotalRunTime) * 100.0 : 0;
					float taskCpuUsageAvg = (totalRunTime > 0) ? ((float)taskStatusArray[i].ulRunTimeCounter / (float)totalRunTime) * 100.0 : 0;
					taskCpuUsageList.push_back({{taskCpuUsage, taskCpuUsageAvg}, taskStatusArray[i]});
				}
				lastSortIndex = -1; // force resort after data refresh
			}

			if (debugMenuSortByIndex != lastSortIndex && !taskCpuUsageList.empty()) {
				lastSortIndex = debugMenuSortByIndex;
				sort(taskCpuUsageList.begin(), taskCpuUsageList.end(), [](const pair<pair<float, float>, TaskStatus_t>& a, const pair<pair<float, float>, TaskStatus_t>& b) {
					int nameCmp = strcmp(a.second.pcTaskName, b.second.pcTaskName);
					switch (debugMenuSortByIndex) {
						case 1: return a.first.second != b.first.second ? a.first.second > b.first.second : nameCmp < 0;
						case 2: return a.second.usStackHighWaterMark != b.second.usStackHighWaterMark ? a.second.usStackHighWaterMark < b.second.usStackHighWaterMark : nameCmp < 0;
						case 3: return nameCmp < 0;
						case 4: return a.second.uxCurrentPriority != b.second.uxCurrentPriority ? a.second.uxCurrentPriority > b.second.uxCurrentPriority : nameCmp < 0;
						default: return a.first.first != b.first.first ? a.first.first > b.first.first : nameCmp < 0;
					}
				});
			}

			if (!taskCpuUsageList.empty()) {
				drawStatsbar();
				sprite.setTextColor(TFT_WHITE);
				sprite.printf("Sort by: %s\n", debugMenuSortByIndex == 0 ? "CPU Usage" : debugMenuSortByIndex == 1 ? "Avg CPU Usage" : debugMenuSortByIndex == 2 ? "Heap Remaining" : debugMenuSortByIndex == 3 ? "Task Name" : "Priority");
				sprite.setTextColor(TFT_CYAN);
				sprite.println("Name            Stats    CPU    Avg    Heap   Prio #");
				sprite.setTextColor(0xC7DE);

				for (const auto& task : taskCpuUsageList) {
					sprite.printf("%-14.14s  %-7.7s  %4.4s%%  %4.4s%%  %-5u  %-2u   %u\n",
						task.second.pcTaskName,
						(task.second.eCurrentState == eRunning)   ? "Running" :
						(task.second.eCurrentState == eReady)     ? "Ready" :
						(task.second.eCurrentState == eBlocked)   ? "Blocked" :
						(task.second.eCurrentState == eSuspended) ? "Paused" :
						(task.second.eCurrentState == eDeleted)   ? "Deleting" : "idk",
						(task.first.first > 99.9) ? " 100" : (task.first.first < 10.0) ? String(task.first.first, 2).c_str() : String(task.first.first, 1).c_str(),
						(task.first.second > 99.9) ? " 100" : (task.first.second < 10.0) ? String(task.first.second, 2).c_str() : String(task.first.second, 1).c_str(),
						task.second.usStackHighWaterMark,
						task.second.uxCurrentPriority,
						(task.second.xCoreID == tskNO_AFFINITY) ? 1 : task.second.xCoreID); // i think i saw the macro for default core but i forgot
				}
			}
			xSemaphoreGive(monMutex);
		}
		sprite.pushSprite(0, 0);
		xSemaphoreGive(drawSemaphore);
	}
}