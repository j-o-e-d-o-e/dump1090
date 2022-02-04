#include <dirent.h>
#include "dump1090.h"
#include "data.h"
#include "dataAPI.h"
#include "constants-flights.h"
#include "local-paths.h"

#define CALL_SIGN_LEN 8
#define ISO_DATE_LEN 64
#define FILENAME_LEN 256
#define JSON_LEN 1024

// ================================ Json ====================================

void trimTrailing(char *t, char *s) {
    strcpy(t, s);
    size_t i = strlen(t) - 1;
    while (i >= 0) {
        if (t[i] == ' ') i--;
        else break;
    }
    t[i + 1] = '\0';
}

char *aircraftToJson(struct aircraft *a, const int altitude, const int speed, time_t now) {
    char callsign[CALL_SIGN_LEN];
    trimTrailing(callsign, a->flight);
    char date_time[ISO_DATE_LEN];
    strftime(date_time, ISO_DATE_LEN, "%FT%T", localtime(&now));
    char *json = (char *) malloc(JSON_LEN);
    snprintf(json, JSON_LEN,
             "{\"icao_24\":\"%06X\",\"callsign\":\"%s\",\"altitude\":%d,\"speed\":%d,\"date_time\":\"%s\"}",
             a->addr, callsign, altitude, speed, date_time);
    return json;
}

void writeToFile(char *json, time_t now) {
    struct tm *date_time = localtime(&now);
    char filename[FILENAME_LEN];
    sprintf(filename, "%s/flights_%04d-%02d-%02d.json", FILE_DIR, date_time->tm_year + 1900, date_time->tm_mon + 1,
            date_time->tm_mday);
    FILE *fp;
    if (access(filename, F_OK) != 0) {
        if ((fp = fopen(filename, "w")) == NULL) {
            fprintf(stderr, "Creating File failed: %s\n", filename);
            exit(EXIT_FAILURE);
        }
        fputs("[\n", fp);
    } else {
        fp = fopen(filename, "r+");
        fseek(fp, -3, SEEK_END);
        fputs(",\n", fp);
    }
    fputs(json, fp);
    fputs("\n]\n", fp);
    fclose(fp);
}

char *readFromFile(time_t now) {
    time_t yesterday = now - 24 * 60 * 60;
    struct tm *date_time = localtime(&yesterday);
    char filename[FILENAME_LEN];
    sprintf(filename, "%s/flights_%04d-%02d-%02d.json", FILE_DIR, date_time->tm_year + 1900, date_time->tm_mon + 1,
            date_time->tm_mday);
    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "Opening File failed: %s\n", filename);
        return "[]";
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
    char *content = malloc(file_size + 1);
    size_t read = fread(content, 1, file_size, fp);
    if (read != file_size) fprintf(stderr, "Error reading from file");
    fclose(fp);
    content[file_size] = 0;
    return content;
}

// =========================== Send/Receive json ===============================

void un_idle_server() {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, URL_UN_IDLE_SERVER);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) fprintf(stderr, "Error: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
}

struct memory {
    char *memory;
    size_t size;
};

static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp) {
//    fprintf(stderr, "Got %lu bytes\n", (int) size * nmemb);
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *) userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1); // one for terminating zero
    if (ptr == NULL) return 0;
    mem->memory = ptr;
    memcpy(&mem->memory[mem->size], contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0; // not necessary
    return size * nmemb;
}

char *key_val(char *line, char *key, int str) {
    char *val = strstr(line, key) + strlen(key);
    if (str) val++;
    char *end = strchr(val, ',');
    if (str) {
        if (end != NULL) val[end - val - 1] = '\0';
        else val[strlen(val) - 1] = '\0';
    } else {
        if (end != NULL) val[end - val] = '\0';
    }
    return val;
}

Data *parse(struct memory *response) {
    strtok(response->memory, "{");
    char tmp[500];
    char *line, *val;
    int count = 0, buffer = 10;
    Data *data = malloc(sizeof(Data) + sizeof(struct flight[buffer]));
    while ((line = strtok(NULL, "{")) != NULL) {
        line[strlen(line) - 2] = '\0';
        if (count == buffer) {
            buffer *= 2;
            data = realloc(data, sizeof(Data) + sizeof(struct flight) * buffer);
        }
        struct flight *flight = &(data->flights[count]);
        // ID
        val = key_val(strcpy(tmp, line), "\"id\":", 0);
        flight->id = strtoul(val, NULL, 10);
        // CALLSIGN
        val = key_val(strcpy(tmp, line), "\"callsign\":", 1);
        strcpy(flight->callsign, val);
        // TIME
        val = key_val(strcpy(tmp, line), "\"date_time\":", 1);
        val += 11;
        val[2] = '-';
        val[5] = '-';
        strcpy(flight->time, val);
        count++;
    }
    data->len = count;
    data = realloc(data, sizeof(Data) + sizeof(struct flight) * data->len);
    return data;
}

Data *httpPost(time_t t, char *content) {
    CURL *curl = curl_easy_init();
    char url[FILENAME_LEN] = FILE_DIR;
    time_t yesterday = t - 24 * 60 * 60;
    struct tm *date_time = localtime(&yesterday);
    sprintf(url, "%s/%04d-%02d-%02d", URL_POST_FLIGHTS, date_time->tm_year + 1900, date_time->tm_mon + 1,
            date_time->tm_mday);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    struct memory response = {.memory = NULL, .size = 0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response); // assigned to userp in callback
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char authorization[256];
    sprintf(authorization, "Authorization: %s", API_KEY);
    headers = curl_slist_append(headers, authorization);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(curl);
    Data *data = NULL;
    if (res != CURLE_OK) {
        fprintf(stderr, "Returned %s\n", curl_easy_strerror(res));
    } else {
        printf("Got %d bytes\n", (int) response.size);
        if (response.size > 0) data = parse(&response);
    }
    free(response.memory);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return data;
}

// =============================== Photos ===================================

int get_timeout(struct aircraft *a, int speed, time_t now, int *secs_since_seen, double *lon_approx, int *dist_to_cam) {
    *secs_since_seen = (int) (now - a->seenLatLon);
    double meters_per_sec = speed / 3.6;
    *lon_approx = secs_since_seen > 0 ?
                  a->lon - (int) (meters_per_sec * *secs_since_seen / DIST_LON) * 0.001
                                      : a->lon;
    *dist_to_cam = (int) ((*lon_approx - CAM_LON) * 1000) * DIST_LON;
    // e.g. 120m / 73.88m/s * 1000 = 1624s
    return (int) (*dist_to_cam / meters_per_sec * 1000);
}

char *takePhoto(struct aircraft *a, int speed, time_t now) {
    char dir[200];
    struct tm *date_time = localtime(&now);
    sprintf(dir, "%s/%04d-%02d-%02d", PHOTO_DIR, date_time->tm_year + 1900, date_time->tm_mon + 1, date_time->tm_mday);
    struct stat st = {0};
    if (stat(dir, &st) == -1) mkdir(dir, 0700);

    double lon_approx;
    int secs_since_seen, dist_to_cam;
    int timeout = get_timeout(a, speed, now, &secs_since_seen, &lon_approx, &dist_to_cam);
    timeout += 250; // adding some ms to delay photo and better center plane

    char command[400] = "";
    char dt[ISO_DATE_LEN];
    strftime(dt, ISO_DATE_LEN, "%FT%T", localtime(&now));
    char callsign[CALL_SIGN_LEN];
    trimTrailing(callsign, a->flight);
    sprintf(command, "raspistill --timeout %d --nopreview -o %s/img_%02d-%02d-%02d_%s.jpg &",
            timeout < 1100 ? 1100 : timeout > 5000 ? 5000 : timeout,
            dir, date_time->tm_hour, date_time->tm_min, date_time->tm_sec, callsign);
    system(command);

    char *timeout_str = (char *) malloc(128);
    sprintf(timeout_str,
            "\"Pos last seen\":%d,\"lon/lat\":%.3f/%.3f,\"lon_approx\":%.3f,\"dist_cam\":%d,\"timeout\":%d",
            secs_since_seen, a->lon, a->lat, lon_approx, dist_to_cam, timeout);
    return timeout_str;
}

void cleanUpPhotos() {
    char command[250] = "";
    sprintf(command, "python3 %s &", PHOTO_CLEAN_UP_PY);
    system(command);
}

void postPhoto(unsigned long id, char *fn) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

    char url[FILENAME_LEN];
    sprintf(url, "%s/%lu/image", URL_POST_IMAGE, id);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");

    struct curl_slist *headers = NULL;
    char authorization[256];
    sprintf(authorization, "Authorization: %s", API_KEY);
    headers = curl_slist_append(headers, authorization);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, "img");

    curl_mime_filedata(part, fn);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Returned %s\n", curl_easy_strerror(res));
    }
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
}

void httpPostPhotos(time_t now, Data *data) {
    time_t yesterday = now - 24 * 60 * 60;
    struct tm *date_time = localtime(&yesterday);
    char photo_dir_date[FILENAME_LEN * 2];
    sprintf(photo_dir_date, "%s/%04d-%02d-%02d/", PHOTO_DIR, date_time->tm_year + 1900, date_time->tm_mon + 1,
            date_time->tm_mday);
    struct flight flight;
    for (int i = 0; i < data->len; i++) {
        flight = data->flights[i];
        char fn[FILENAME_LEN * 2];
        sprintf(fn, "%s/img_%s_%s.jpg", photo_dir_date, flight.time, flight.callsign);
        if (access(fn, F_OK) == 0) postPhoto(flight.id, fn);
    }
}
