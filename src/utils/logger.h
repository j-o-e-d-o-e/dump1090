#pragma once

#include "../main.h"

#define LOG_FILENAME_MAX_LEN() strlen(FILE_DIR) + 22
#define LOG_ENTRY_INTRO_MAX_METHOD(s) strlen(s) + 10
#define LOG_ENTRY_INTRO_DT_LVL_MAX_LEN 20

enum loggingLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
};

void writeLogEntry(enum loggingLevel lvl, char *method, int n, ...);
