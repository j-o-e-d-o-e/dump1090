#include "test-framework/unity.h"
#include "test-framework/unity_fixture.h"
#include <malloc.h>
#include <time.h>
#include "../src/connect.h"

TEST_GROUP(connect);
#define PATH_TEST_FILE "/home/joe/prog/c/traffic-tracker/v3/traffic_tracker/flights/1970-01-01.json"
#define EXP_JSON_STRING "{\"icao_24\":\"4008E6\",\"callsign\":\"EWG8RA\",\"altitude\":600,\"speed\":300,\"date_time\":\"1970-01-01T01:00:00\"}"
struct aircraft a = {
        .addr = 4196582,
        .flight= "EWG8RA",
        .altitude = 600,
        .speed = 300,
        .seen = 0
};

TEST_SETUP(connect) {
}

TEST_TEAR_DOWN(connect) {
}

TEST(connect, aircraftToJson) {
    char json[JSON_MAX_LEN];
    aircraftToJson(&a, json);
    TEST_ASSERT_EQUAL_STRING(EXP_JSON_STRING, json);
}

TEST(connect, writeJsonToFile) {
    char json[JSON_MAX_LEN];
    aircraftToJson(&a, json);
    writeJsonToFile(json, 0);
}

TEST(connect, readFromFile) {
    time_t dayAfter = a.seen + 24 * 60 * 60;
    char *content = readFromFile(dayAfter);
    int maxLen = strlen(EXP_JSON_STRING) + 6;
    char exp[maxLen];
    snprintf(exp, maxLen, "[\n%s\n]\n", EXP_JSON_STRING);
    TEST_ASSERT_EQUAL_STRING(exp, content);
    free(content);
}

TEST(connect, unIdleServer) {
    TEST_IGNORE();
    unIdleServer();
}

TEST(connect, httpPostJson) {
    TEST_IGNORE(); // change URL_POST_FLIGHTS in connect_api.h if test is not ignored
    time_t dayAfter = a.seen + 24 * 60 * 60;
    char *json = readFromFile(dayAfter);
    Data *data = httpPostJson(json, dayAfter);
    free(json);
    TEST_ASSERT_NULL(data);
}

TEST(connect, deleteTestFile) {
    if (remove(PATH_TEST_FILE) != 0) perror("Deleting test file failed");
}

TEST(connect, takePhoto) {
    TEST_IGNORE();
    takePhoto(&a, time(NULL));
}

TEST(connect, cleanUpPhotos) {
    TEST_IGNORE();
    cleanUpPhotos();
}

TEST(connect, httpPostPhotos) {
    TEST_IGNORE(); // change URL_POST_PHOTO in connect_api.h if test is not ignored
    Data *data = malloc(sizeof(*data) + sizeof(struct flight[10]));
    data->len = 10;
    for (unsigned char i = 0; i < 10; i++) {
        struct flight *flight = &(data->flights[i]);
        flight->id = i;
        snprintf(flight->callsign, 20, "EWG8RA");
        snprintf(flight->time, 20, "970-01-01T01:00:00");
    }
    httpPostPhotos(data, 0);
    free(data);
}
