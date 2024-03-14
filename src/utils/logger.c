#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"

char *filename(struct tm *dt) {
    int maxLen = LOG_FILENAME_MAX_LEN();
    char *fn = malloc(sizeof(char) * maxLen);
    snprintf(fn, maxLen, "%s/logs/%04d-%02d-%02d.log", ROOT_DIR, dt->tm_year + 1900, dt->tm_mon + 1,
             dt->tm_mday);
    return fn;
}

FILE *openFile(char *fn) {
    FILE *fp;
    if (access(fn, F_OK) != 0) fp = fopen(fn, "w+");
    else {
        fp = fopen(fn, "r+");
        fseek(fp, -1, SEEK_END);
    }
    free(fn);
    if (fp == NULL) {
        fprintf(stderr, "Opening File failed: %s\n", fn);
        exit(EXIT_FAILURE);
    }
    return fp;
}

char *logEntryIntro(struct tm *dt, enum loggingLevel lvl, char *method) {
    int maxLenMethod = LOG_ENTRY_INTRO_MAX_METHOD(method);
    int maxLenIntro = LOG_ENTRY_INTRO_DT_LVL_MAX_LEN + maxLenMethod;
    char *intro = malloc( maxLenIntro);
    snprintf(intro, maxLenIntro, "%02d:%02d:%02d, ", dt->tm_hour, dt->tm_min, dt->tm_sec);
    switch (lvl) {
        case LOG_DEBUG:
            strcat(intro, "DEBUG");
            break;
        case LOG_INFO:
            strcat(intro, "INFO");
            break;
        case LOG_WARNING:
            strcat(intro, "WARNING");
            break;
        case LOG_ERROR:
            strcat(intro, "ERROR");
    }
    char *mLog = malloc(sizeof(char) * maxLenMethod);
    snprintf(mLog, maxLenMethod, " [%s]: ", method);
    strcat(intro, mLog);
    free(mLog);
    return intro;
}

void writeLogEntry(enum loggingLevel lvl, char *method, int n, ...) {
    time_t now = time(NULL);
    struct tm *dt = localtime(&now);
    char *fn = filename(dt);
    FILE *fp = openFile(fn);
    char *intro = logEntryIntro(dt, lvl, method);
    fputs(intro, fp);
    free(intro);
    va_list va;
    va_start(va, n);
    for (int i = 0; i < n; i++) {
        char *s = va_arg(va, char *);
        fprintf(fp, "%s, ", s);
    }
    va_end(va);
    fseek(fp, -2, SEEK_END);
    fputc('\n', fp);
    fclose(fp);
}
