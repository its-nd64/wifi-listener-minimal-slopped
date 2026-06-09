#pragma once
#include "Arduino.h"
#include "log.h"
#include "OneButton.h"
#include "OneButtonTiny.h"
#include "mon_thing.h"
#include "display.h"
#include "esp_wifi.h"

// fsr i feel like OneButtonTiny is clunky and slower, TODO: test
OneButton button1(21); // change menu
OneButton button2(22); // change submenu, additional functions

void waitForAnyButtonPress() {
	int pressed = 69;
	while (pressed == 69) {
		if (!digitalRead(21)) pressed = 21;
		else if (!digitalRead(22)) pressed = 22;
		delay(20);
	}
	while (!digitalRead(pressed)) delay(10);
}

void init_button() {
	button1.setClickMs(50);
	button1.setPressMs(350);
	button2.setClickMs(50);
	button2.setPressMs(600);
 
	button1.attachClick([]() { // 0->1->3->0, dont go to 3 if enable debug menu is false
		menuIndex = (menuIndex == 0) ? 1 : (menuIndex == 1) ? (enableDebugMenu ? 3 : 0) : 0;
		display_draw();
	});
	button1.attachLongPressStart([]() {
		deauthActive = !deauthActive;
	});
	button2.attachClick([]() {
		if (!menuIndex) {
			if (!channels.empty()) {
				channelIndex = (channelIndex + 1) % channels.size();
				esp_wifi_set_channel(channels[channelIndex], WIFI_SECOND_CHAN_NONE);
			}
		} else if (menuIndex == 1) menuIndex = 2;
		else if (menuIndex == 2) menuIndex = 1;
		else debugMenuSortByIndex = (debugMenuSortByIndex + 1) % 5;
		display_draw();
	});
	button2.attachLongPressStart([]() {
		haveSD = false;
		flushWriteBufSafe();
		log1(COLOR_NO_SD, "Stopped SD logging!");
	});
}
