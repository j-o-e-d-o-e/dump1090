#include "test-framework/unity.h"
#include "test-framework/unity_fixture.h"
#include <malloc.h>
#include <time.h>
#include "../src/connect.h"

TEST_GROUP(connect);

TEST_SETUP(connect) {
}

TEST_TEAR_DOWN(connect) {
}

TEST(connect, aircraftToJson) {
    struct aircraft *a = malloc(sizeof *a);
    memset(a, 0, sizeof(*a));
    memset(a->flight, '6', 2 * sizeof(char));
    a->addr = 12;
    char *res = aircraftToJson(a, 0, 300, 0);
    free(a);
    printf("res: %s\n", res);
    free(res);
}

TEST(connect, httpPost) {
    TEST_IGNORE();
    // char content[100] = "{'icao_24':'00000C','callsign':'abc66','altitude':0,'speed':300,'date_time':'1970-01-01T01:00:00'}";
    // Data *data = httpPost(time(NULL), content);
    // printf("%d", data->len);
}

TEST(connect, writeToFile) {
    TEST_IGNORE();
    struct aircraft *a = malloc(sizeof *a);

    snprintf(a->flight, 16, "%s", "3C5EE8");
    snprintf(a->flight, 16, "%s", "3C5EE8");
    time_t yesterday = time(NULL) - (60 * 60 * 24);
    char *json = aircraftToJson(a, 883, 353, yesterday);
    free(a);
    writeToFile(json, yesterday);
}

TEST(connect, readFromFile) {
    TEST_IGNORE();
    time_t now = time(NULL);
    char *content = readFromFile(now);
    puts(content);
    free(content);
}
