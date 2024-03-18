#include "dump1090.h"
#include "connect.h"
#include "connect_api.h"
#include "utils/logger.h"
#include "utils/utils.h"

// ================================ Json ====================================

void aircraftToJson(struct aircraft *a, char *json) {
    char callsign[strlen(a->flight) + 1];
    trim(callsign, a->flight);
    char date_time[ISO_DATE_MAX_LEN];
    strftime(date_time, ISO_DATE_MAX_LEN, "%FT%T", localtime(&a->seen));
    snprintf(json, JSON_MAX_LEN,
             "{\"icao_24\":\"%06X\",\"callsign\":\"%s\",\"altitude\":%d,\"speed\":%d,\"date_time\":\"%s\"}",
             a->addr, callsign, a->altitude, a->speed, date_time);
}

void writeJsonToFile(char *json, time_t now) {
    struct tm *date_time = localtime(&now);
    char relPath[FN_RELATIVE_PATH_MAX_LEN];
    snprintf(relPath, FN_RELATIVE_PATH_MAX_LEN, "flights/%04d-%02d-%02d.json",
             date_time->tm_year + 1900, date_time->tm_mon + 1, date_time->tm_mday);
    char filename[FN_ABS_PATH_MAX_LEN()];
    snprintf(filename, FN_ABS_PATH_MAX_LEN(), "/%s/%s", ROOT_DIR, relPath);
    FILE *fp;
    if (access(filename, F_OK) != 0) {
        if ((fp = fopen(filename, "w")) == NULL) {
            writeLogEntry(3, "writeJsonToFile", 2, "Creating File failed", relPath);
            exit(EXIT_FAILURE);
        }
        fputs("[\n", fp);
    } else {
        fp = fopen(filename, "r+");
        if (fp == NULL) {
            writeLogEntry(3, "writeJsonToFile", 2, "Opening File failed", relPath);
        }
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
    char relPath[FN_RELATIVE_PATH_MAX_LEN];
    snprintf(relPath, FN_RELATIVE_PATH_MAX_LEN, "flights/%04d-%02d-%02d.json",
             date_time->tm_year + 1900, date_time->tm_mon + 1, date_time->tm_mday);
    char filename[FN_ABS_PATH_MAX_LEN()];
    snprintf(filename, FN_ABS_PATH_MAX_LEN(), "/%s/%s", ROOT_DIR, relPath);
    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL) {
        writeLogEntry(3, "readFromFile", 2, "Opening File failed", relPath);
        return "[]";
    }
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);
    char *content = malloc(sizeof(char) * file_size + 1);
    size_t read = fread(content, 1, file_size, fp);
    if (read != file_size) {
        free(content);
        writeLogEntry(3, "readFromFile", 2, "Reading File failed", relPath);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    content[file_size] = 0;
    return content;
}

// =========================== Send/Receive Json ===============================

void unIdleServer(void) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, URL_UN_IDLE_SERVER);
    CURLcode ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) writeLogEntry(3, "unIdleServer", 2, "Un-idling server failed", curl_easy_strerror(ret));
    curl_easy_cleanup(curl);
}

static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    struct memory *mem = (struct memory *) userp;
    char *ptr = realloc(mem->memory, mem->size + real_size + 1);
    if (ptr == NULL) return 0;
    mem->memory = ptr;
    memcpy(&mem->memory[mem->size], contents, real_size);
    mem->size += real_size;
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
    Data *data = malloc(sizeof(*data) + sizeof(struct flight[buffer]));
    while ((line = strtok(NULL, "{")) != NULL) {
        line[strlen(line) - 2] = '\0';
        if (count == buffer) {
            buffer *= 2;
            Data *tmp_data = realloc(data, sizeof(Data) + sizeof(struct flight) * buffer);
            if (tmp_data == NULL) {
                free(data);
                free(response->memory);
                writeLogEntry(3, "parse", 1, "Parsing received data failed");
                exit(EXIT_FAILURE);
            }
            data = tmp_data;
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
    Data *tmp_data = realloc(data, sizeof(Data) + sizeof(struct flight) * data->len);
    if (tmp_data == NULL) {
        free(data);
        free(response->memory);
        writeLogEntry(3, "parse", 1, "Parsing received data failed");
        exit(EXIT_FAILURE);
    }
    data = tmp_data;
    char msg[30];
    snprintf(msg, 30, "%d flights", data->len);
    writeLogEntry(1, "parse", 2, "Parsing received data succeeded", msg);
    return data;
}

Data *httpPostJson(char *json, time_t now) {
    CURL *curl = curl_easy_init();
    int maxLen = strlen(URL_POST_FLIGHTS) + 40;
    char url[maxLen];
    time_t yesterday = now - 24 * 60 * 60;
    struct tm *date_time = localtime(&yesterday);
    snprintf(url, maxLen, "%s/%04d-%02d-%02d",
             URL_POST_FLIGHTS, date_time->tm_year + 1900, date_time->tm_mon + 1, date_time->tm_mday);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    struct memory response = {.memory = NULL, .size = 0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response); // assigned to userp in callback
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    maxLen = strlen(API_KEY) + 20;
    char auth[maxLen];
    snprintf(auth, maxLen, "Authorization: %s", API_KEY);
    headers = curl_slist_append(headers, auth);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode ret = curl_easy_perform(curl);
    Data *data = NULL;
    if (ret != CURLE_OK) {
        writeLogEntry(3, "httpPostJson", 3, "Sending data failed", url, curl_easy_strerror(ret));
    } else {
        char msg[30];
        snprintf(msg, 30, "Got %zu bytes back", response.size);
        writeLogEntry(1, "httpPostJson", 2, "Sending data succeeded", msg);
        if (response.size > 0) data = parse(&response);
    }
    free(response.memory);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return data;
}

// =============================== Photos ===================================

int getTimeout(struct aircraft *a, int speed, time_t now) {
    const float CAM_LON = 6.961f;
    const int DIST_LON = 70; // alternatively, 75 or even 80
    int secs_since_seen = (int) (now - a->seenLatLon);
    double meters_per_sec = speed / 3.6;
    double lon_approx = secs_since_seen > 0 ?
                        a->lon - (int) (meters_per_sec * secs_since_seen / DIST_LON) * 0.001 : a->lon;
    int dist_to_cam = (int) ((lon_approx - CAM_LON) * 1000) * DIST_LON;
    return (int) (dist_to_cam / meters_per_sec * 1000); // e.g. 120m / 73.88m/s * 1000 = 1624s
}

void takePhoto(struct aircraft *a, time_t now) {
    int maxLen = FN_ABS_PATH_MAX_LEN();
    char dir[maxLen];
    struct tm *date_time = localtime(&a->seen);
    snprintf(dir, maxLen, "%s/photos/%04d-%02d-%02d",
             ROOT_DIR, date_time->tm_year + 1900, date_time->tm_mon + 1, date_time->tm_mday);
    struct stat st = {0};
    if (stat(dir, &st) == -1) mkdir(dir, 0700);

    int timeout = getTimeout(a, a->speed, now);
    timeout += 250; // adding some ms to delay photo and better center plane
    char callsign[strlen(a->flight) + 1];
    trim(callsign, a->flight);
    char command[COMMAND_MAX_LEN];
    snprintf(command, COMMAND_MAX_LEN, "raspistill --timeout %d --nopreview -o %s/%02d-%02d-%02d_%s.jpg &",
             timeout < 1100 ? 1100 : timeout > 5000 ? 5000 : timeout,
             dir, date_time->tm_hour, date_time->tm_min, date_time->tm_sec, callsign);
    system(command);
}

void cleanUpPhotos(void) {
    char command[COMMAND_MAX_LEN];
    snprintf(command, COMMAND_MAX_LEN, "python3 %s/py/clean_up_photos.py &", ROOT_DIR);
    system(command);
    writeLogEntry(1, "cleanUpPhotos", 1, command);
}

// =========================== Send Photos ===============================

unsigned char postPhoto(unsigned long id, char *fn) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

    int maxLen = strlen(URL_POST_PHOTO) + 40;
    char url[maxLen];
    snprintf(url, maxLen, "%s/%lu/image", URL_POST_PHOTO, id);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");

    struct curl_slist *headers = NULL;
    maxLen = strlen(API_KEY) + 20;
    char auth[maxLen];
    snprintf(auth, maxLen, "Authorization: %s", API_KEY);
    headers = curl_slist_append(headers, auth);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, "img");

    curl_mime_filedata(part, fn);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    CURLcode ret = curl_easy_perform(curl);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    if (ret != CURLE_OK) {
        writeLogEntry(3, "postPhoto", 4, "Sending photo failed", fn, url, curl_easy_strerror(ret));
        return 0;
    } else return 1;
}

void httpPostPhotos(Data *data, time_t now) {
    int maxLen = FN_ABS_PATH_MAX_LEN();
    char dir[maxLen];
    time_t yesterday = now - 24 * 60 * 60;
    struct tm *date_time = localtime(&yesterday);
    snprintf(dir, maxLen, "%s/photos/%04d-%02d-%02d",
             ROOT_DIR, date_time->tm_year + 1900, date_time->tm_mon + 1, date_time->tm_mday);
    struct flight flight;
    int total = 0, success = 0, fail = 0;
    maxLen += 30;
    for (int i = 0; i < data->len; i++) {
        flight = data->flights[i];
        char fn[maxLen];
        snprintf(fn, maxLen, "%s/%s_%s.jpg", dir, flight.time, flight.callsign);
        if (access(fn, F_OK) == 0) {
            unsigned char ret = postPhoto(flight.id, fn);
            if (ret) success++;
        } else fail++;
        total++;
    }
    char summary[40];
    snprintf(summary, 40, "success+deleted/total: %d+%d/%d", success, fail, total);
    writeLogEntry(1, "httpPostPhotos", 2, dir, summary);
}
