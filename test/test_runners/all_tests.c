#include "../test-framework/unity_fixture.h"

static void RunAllTests(void) {
    RUN_TEST_GROUP(connect);
    RUN_TEST_GROUP(utils);
}

int main(int argc, const char *argv[]) {
    return UnityMain(argc, argv, RunAllTests);
}
