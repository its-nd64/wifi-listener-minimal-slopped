#pragma once
#include <Arduino.h>
#include "config.h"

struct LogEntry { char message[MAX_LOG_LEN]; uint16_t color; };

struct LogChannel {
    QueueHandle_t queue;
    LogEntry ring[LOG_RING_SIZE];
    int head = 0, size = 0;
};

LogChannel logCh1, logCh2;

// aliases for display.h and elsewhere
#define logList1    logCh1.ring
#define logListHead1 logCh1.head
#define logListSize1 logCh1.size
#define logList2    logCh2.ring
#define logListHead2 logCh2.head
#define logListSize2 logCh2.size

static void pushEntry(LogChannel& ch, const LogEntry& entry) {
    ch.ring[(ch.head + ch.size) % LOG_RING_SIZE] = entry;
    if (ch.size < LOG_RING_SIZE) ch.size++;
    else ch.head = (ch.head + 1) % LOG_RING_SIZE;
}

static void init_log(LogChannel& ch, const char* taskName) {
    ch.queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogEntry));

    xTaskCreatePinnedToCore([](void* arg) {
        LogChannel& ch = *(LogChannel*)arg;
        char lastMessage[MAX_LOG_LEN] = "";
        int repeatCount = 1;

        while (1) {
            LogEntry entry;
            if (xQueueReceive(ch.queue, &entry, portMAX_DELAY) != pdTRUE) continue;

            if (strncmp(entry.message, lastMessage, MAX_LOG_LEN) == 0) {
                repeatCount++;
                snprintf(ch.ring[(ch.head + ch.size - 1) % LOG_RING_SIZE].message, MAX_LOG_LEN, "%s x%d", lastMessage, repeatCount);
            } else {
                repeatCount = 1;
                strncpy(lastMessage, entry.message, MAX_LOG_LEN);

                if (strchr(entry.message, '\n')) {
                    const char* p = entry.message;
                    while (*p) {
                        LogEntry part;
                        part.color = entry.color;
                        int i = 0;
                        while (i < MAX_LOG_LEN - 1 && *p && *p != '\n') part.message[i++] = *p++;
                        part.message[i] = '\0';
                        if (*p == '\n') p++;
                        pushEntry(ch, part);
                    }
                } else {
                    pushEntry(ch, entry);
                }
            }
        }
    }, taskName, LOG_TASK_STACK_SIZE, &ch, 3, NULL, LOG_TASK_CORE);
}

void init_log1() { init_log(logCh1, "log1"); }
void init_log2() { init_log(logCh2, "log2"); }

static void logTo(LogChannel& ch, uint16_t color, const char* fmt, va_list args) {
    LogEntry entry;
    entry.color = color;
    vsnprintf(entry.message, MAX_LOG_LEN, fmt, args);
    xQueueSend(ch.queue, &entry, 0); // don't block
}

void log1(uint16_t color, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    logTo(logCh1, color, fmt, args);
    va_end(args);
}

void log2(uint16_t color, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    logTo(logCh2, color, fmt, args);
    va_end(args);
}

template<typename T> void log1(uint16_t, const T&) { static_assert(sizeof(T) == 0, "ONLY CHAR* ALLOWED"); }
template<typename T> void log2(uint16_t, const T&) { static_assert(sizeof(T) == 0, "ONLY CHAR* ALLOWED"); }
