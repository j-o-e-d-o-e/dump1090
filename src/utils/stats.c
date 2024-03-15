#include "stats.h"
#include <stdio.h>


struct Stats stats;

void setStatsStartTime(void) {
    stats.start_time = time(NULL);
    struct tm *start_time_tm = localtime(&stats.start_time);
    strftime(stats.start_time_str, sizeof(stats.start_time_str), "%a, %b %d, %H:%Mh", start_time_tm);
}

void increaseStatsTotal(void) {
    stats.total++;
    stats.total_changed = 1;
}

void resetStatsTotal(void){
    stats.total = 0;
    stats.total_changed = 1;
}

int minutesPassedToday(time_t now) {
    struct tm *now_tm = localtime(&now);
    struct tm *start_time_tm = localtime(&stats.start_time);
    time_t time0;
    if (now_tm->tm_yday > start_time_tm->tm_yday || now_tm->tm_year > start_time_tm->tm_year) { // next day
        now_tm->tm_hour = 5;
        now_tm->tm_min = 55;
        now_tm->tm_sec = 0;
        time0 = mktime(now_tm);
    } else time0 = stats.start_time;
    return (int) difftime(now, time0) / 60;
}

void setAverage(const int total, const int diff_min) {
    if (diff_min > 0) stats.avg = (float) total / ((float) diff_min / 60.0f);
}

void setFrequency(const int total, const int diff_min) {
    if (total > 0) stats.freq = (float) diff_min / ((float) total);
}

void printStats(time_t now) {
    if (stats.total_changed) {
        stats.total_changed = 0;
        int diff_min = minutesPassedToday(now);
        setAverage(stats.total, diff_min);
        setFrequency(stats.total, diff_min);
    }
    printf("Total/day: %8d | Freq/min: %6.2f | Avg/h: %6.2f | Start: %s\n",
           stats.total, stats.freq, stats.avg, stats.start_time_str);
}
