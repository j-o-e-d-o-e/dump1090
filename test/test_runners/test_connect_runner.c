#include "../test-framework/unity_fixture.h"

TEST_GROUP_RUNNER(connect) {
    RUN_TEST_CASE(connect, aircraftToJson);
    RUN_TEST_CASE(connect, writeJsonToFile);
    RUN_TEST_CASE(connect, readFromFile);
    RUN_TEST_CASE(connect, httpPost);
    RUN_TEST_CASE(connect, deleteTestFile);
}
