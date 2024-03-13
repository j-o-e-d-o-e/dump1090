#include "test-framework/unity.h"
#include "test-framework/unity_fixture.h"
#include <malloc.h>
#include <time.h>
#include "../src/connect.h"

TEST_GROUP(connect);
#define PATH_TEST_FILE "/home/joe/prog/c/traffic-tracker/v3/traffic_tracker/flights/1970-01-01.json"
#define EXP_JSON_STRING "{\"icao_24\":\"4008E6\",\"callsign\":\"EWG8RA\",\"altitude\":600,\"speed\":300,\"date_time\":\"1970-01-01T01:00:00\"}"
struct aircraft *a;

TEST_SETUP(connect) {
    a = malloc(sizeof(*a));
    a->addr = 4196582;
    memcpy(a->flight, "EWG8RA", sizeof(a->flight));
    a->altitude = 600;
    a->speed = 300;
    a->seen = 0;
}

TEST_TEAR_DOWN(connect) {
    free(a);
}

TEST(connect, aircraftToJson) {
    char json[JSON_MAX_LEN];
    aircraftToJson(a, json);
    TEST_ASSERT_EQUAL_STRING(EXP_JSON_STRING, json);
}

TEST(connect, writeJsonToFile) {
    // TEST_IGNORE();
    char json[JSON_MAX_LEN];
    aircraftToJson(a, json);
    writeJsonToFile(json, 0);
}

TEST(connect, readFromFile) {
    // TEST_IGNORE();
    time_t dayAfter = a->seen + 24 * 60 * 60;
    char *content = readFromFile(dayAfter);
    int maxLen = strlen(EXP_JSON_STRING) + 6;
    char exp[maxLen];
    snprintf(exp, maxLen, "[\n%s\n]\n", EXP_JSON_STRING);
    if (remove(PATH_TEST_FILE) != 0) perror("Deleting test file failed");
    TEST_ASSERT_EQUAL_STRING(exp, content);
    free(content);
}

TEST(connect, httpPost) {
    TEST_IGNORE();
    // char content[100] = "{'icao_24':'00000C','callsign':'abc66','altitude':0,'speed':300,'date_time':'1970-01-01T01:00:00'}";
    // Data *data = httpPost(time(NULL), content);
    // printf("%d", data->len);
}
