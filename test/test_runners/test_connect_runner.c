#include "../test-framework/unity_fixture.h"

TEST_GROUP_RUNNER(connect) {
    RUN_TEST_CASE(connect, aircraftToJson);
    RUN_TEST_CASE(connect, writeJsonToFile);
    RUN_TEST_CASE(connect, readFromFile);
    RUN_TEST_CASE(connect, unIdleServer);
    RUN_TEST_CASE(connect, httpPostJson);
    RUN_TEST_CASE(connect, deleteTestFile);
    RUN_TEST_CASE(connect, takePhoto);
    RUN_TEST_CASE(connect, cleanUpPhotos);
    RUN_TEST_CASE(connect, httpPostPhotos);
}
