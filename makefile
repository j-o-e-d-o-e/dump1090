CC = gcc
#CFLAGS = -std=c99
CFLAGS = -std=c11
CFLAGS += -g
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -pedantic
CFLAGS += -Werror
#CFLAGS += -Wmissing-declarations
CFLAGS +=-DUNITY_FIXTURE_NO_EXTRAS

ASANFLAGS  = -fsanitize=address
ASANFLAGS += -fno-common
ASANFLAGS += -fno-omit-frame-pointer

LIBS = -lpthread -lcurl -lm
LIBS+= `pkg-config --libs librtlsdr`

SRC_FILES := $(wildcard src/*.c)
SRC_TARGET = bin/app.out
UNITY_DIR  = test/test-framework
INC_DIRS   = -Isrc -I$(UNITY_DIR)
TEST_FILES:= $(filter-out src/main.c, \
	$(wildcard src/*.c test/*.c test/test_runners/*.c \
  $(UNITY_DIR)/unity.c $(UNITY_DIR)/unity_fixture.c))
TEST_TARGET = bin/test.out
MEM_CHECK_TARGET = bin/memcheck.out

.PHONY: all
all: app

.PHONY: app
app: $(SRC_TARGET)

$(SRC_TARGET): $(SRC_FILES)
	@$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

.PHONY: test
test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_FILES)
	@$(CC) $(CFLAGS) $(INC_DIRS) $^ -o $@ $(LIBS)

.PHONY: memcheck
memcheck: $(MEM_CHECK_TARGET)

$(MEM_CHECK_TARGET): $(SRC_FILES)
	@$(CC) $(CFLAGS) $(ASANFLAGS) $^ -o $@ $(LIBS)

.PHONY: clean
clean:
	@rm -f $(MEM_CHECK_TARGET) $(TEST_TARGET) $(SRC_TARGET) json/*.json
