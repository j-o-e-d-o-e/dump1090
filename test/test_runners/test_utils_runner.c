#include "../test-framework/unity_fixture.h"

TEST_GROUP_RUNNER(utils) {
    RUN_TEST_CASE(utils, writeLogEntry);
    RUN_TEST_CASE(utils, trim);
}
