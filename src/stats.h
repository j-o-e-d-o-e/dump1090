#pragma once

#include <time.h>
#include <stdint.h>

struct Stats {
    time_t start_time;
    char start_time_str[20];
    int total;
    unsigned char total_changed;
    float avg;
    float freq;
};

void setStartTime(void);

void increaseTotal(void);

void printStats(time_t now);
