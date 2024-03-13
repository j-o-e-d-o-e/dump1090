#include <string.h>
#include "test-framework/unity_fixture.h"
#include "../src/utils/logger.h"
#include "../src/utils/utils.h"

TEST_GROUP(utils);

TEST_SETUP(utils) {
}

TEST_TEAR_DOWN(utils) {
}

TEST(utils, writeLogEntry) {
    TEST_IGNORE();
    writeLogEntry(0, "main", 2, "App started", "Test results in DEBUG");
    writeLogEntry(1, "TEST", 2, "App started", "Test results in INFO");
    writeLogEntry(2, "main main main main", 1, "Test results in WARNING");
    writeLogEntry(3, "TEST TEST TEST TEST", 1, "Test results in ERROR");
}

TEST(utils, trim) {
    char *src = "   abc   ";
    char dest[strlen(src)];
    trim(dest, src);
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}
