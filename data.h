#ifndef FLIGHT_TRACKER_DATA_H
#define FLIGHT_TRACKER_DATA_H

#include <curl/curl.h>
#include "dump1090.h"

struct flight {
    unsigned long id;
    char callsign[20];
    char time[20];
};

typedef struct {
    int len;
    struct flight flights[];
} Data;

// Returns description of an aircraft in json
char *aircraftToJson(struct aircraft *a, int altitude, int speed, time_t now);

void writeToFile(char *json, time_t now);

char *readFromFile(time_t now);

void un_idle_server();

Data *httpPost(time_t now, char *json);

char *takePhoto(struct aircraft *a, int speed, time_t now);

void cleanUpPhotos(void);

void httpPostPhotos(time_t now, Data *data);

#endif //FLIGHT_TRACKER_DATA_H
