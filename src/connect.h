#pragma once

#include <curl/curl.h>
#include "dump1090.h"

#ifdef __arm__
#define FILE_DIR "/home/pi/Desktop/flight_tracker/json"
#define PHOTO_DIR "/home/pi/Desktop/flight_tracker/photos"
#define PHOTO_CLEAN_UP_PY "/home/pi/Desktop/flight_tracker_img/cv2_img_ft.py"
#else
#define FILE_DIR "/home/joe/prog/c/traffic-tracker/v3/traffic_tracker/json"
#define PHOTO_DIR ""
#define PHOTO_CLEAN_UP_PY ""
#endif
#define CALL_SIGN_LEN 8
#define ISO_DATE_LEN 64
#define FILENAME_LEN 256
#define JSON_LEN 1024
#define CAM_LON 6.961
#define DIST_LON 70 // alternatively, 75 or even 80

struct flight {
    unsigned long id;
    char callsign[20];
    char time[20];
};

typedef struct {
    int len;
    struct flight flights[];
} Data;

char *aircraftToJson(struct aircraft *a, int altitude, int speed, time_t now);

void writeToFile(char *json, time_t now);

char *readFromFile(time_t now);

void un_idle_server(void);

Data *httpPost(time_t now, char *json);

void takePhoto(struct aircraft *a, int speed, time_t now);

void cleanUpPhotos(void);

void httpPostPhotos(time_t now, Data *data);
