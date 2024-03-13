#pragma once
#ifdef __arm__
#define LOG_FILE_DIR "/home/pi/Desktop/flight_tracker/logs"
#else
#define LOG_FILE_DIR "/home/joe/prog/c/traffic-tracker/v3/traffic_tracker/logs"
#endif
#define LOG_FILENAME_MAX_LEN() strlen(LOG_FILE_DIR) + 20
#define LOG_ENTRY_INTRO_MAX_METHOD(s) strlen(s) + 10
#define LOG_ENTRY_INTRO_DT_LVL_MAX_LEN 20

enum loggingLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
};

void writeLogEntry(enum loggingLevel lvl, char *method, int n, ...);
