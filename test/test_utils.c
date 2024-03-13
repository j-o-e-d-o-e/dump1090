#include "test-framework/unity_fixture.h"
#include "../src/logger.h"
#include "../src/connect.h"

TEST_GROUP(utils);

TEST_SETUP(utils) {
}

TEST_TEAR_DOWN(utils) {
}

TEST(utils, writeLogEntry) {
    TEST_IGNORE();
    writeLogEntry(0, "main", 2,  "App started...", "next initializing components...");
    writeLogEntry(1, "TEST", 2,  "App started...", "next initializing components...");
    writeLogEntry(2, "main main main main", 1,  "Test results in warning");
    writeLogEntry(3, "TEST TEST TEST TEST", 1,  "Test results in error");
}

TEST(utils, trim) {
    char *src = "   assd   ";
    char dest[strlen(src)];
    trim(dest, src);
    puts(dest);
}
