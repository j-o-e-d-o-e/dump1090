#pragma once

#include <curl/curl.h>
#include "main.h"
#include "dump1090.h"

#ifdef __arm__
#define PHOTO_DIR  FILE_DIR "/photos"
#define PHOTO_CLEAN_UP_PY "/home/pi/Desktop/flight_tracker_img/cv2_img_ft.py"
#else
#define PHOTO_DIR ""
#define PHOTO_CLEAN_UP_PY ""
#endif
#define ISO_DATE_MAX_LEN 20
#define JSON_MAX_LEN 128
#define FN_RELATIVE_PATH_MAX_LEN 25
#define FN_ABS_PATH_MAX_LEN() strlen(FILE_DIR) + FN_RELATIVE_PATH_MAX_LEN + 2
#define CAM_LON 6.961
#define DIST_LON 70 // alternatively, 75 or even 80

struct flight {
    unsigned long id;
    char callsign[20];
    char time[20];
};

struct memory {
    char *memory;
    size_t size;
};

typedef struct {
    int len;
    struct flight flights[];
} Data;

void aircraftToJson(struct aircraft *a, char *json);

void writeJsonToFile(char *json, time_t now);

char *readFromFile(time_t now);

void unIdleServer(void);

Data *httpPostJson(char *json, time_t now);

void takePhoto(struct aircraft *a, time_t now);

void cleanUpPhotos(void);

void httpPostPhotos(time_t now, Data *data);
