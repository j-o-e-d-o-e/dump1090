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

TEST(connect, httpPostJson) {
    TEST_IGNORE(); // change URL_POST_FLIGHTS in connect_api.h if test is active
    time_t dayAfter = a.seen + 24 * 60 * 60;
    char *json = readFromFile(dayAfter);
    Data *data = httpPostJson(json, dayAfter);
    free(json);
    TEST_ASSERT_NULL(data);
}

TEST(connect, deleteTestFile) {
    if (remove(PATH_TEST_FILE) != 0) perror("Deleting test file failed");
}
