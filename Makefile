CXX ?= g++
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=

# Detect Operating System
ifeq ($(OS),Windows_NT)
    TARGET_SERVER = mini_redis.exe
    TARGET_TEST = test_suite.exe
    LDFLAGS += -lws2_32
    RM = del /Q /F
else
    TARGET_SERVER = mini_redis
    TARGET_TEST = test_suite
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -pthread
        LDFLAGS += -pthread
    endif
    RM = rm -f
endif

COMMON_SRCS = src/core/storage_engine.cpp src/protocol/resp_parser.cpp src/persistence/wal.cpp
SERVER_SRCS = src/main.cpp src/network/server.cpp $(COMMON_SRCS)
TEST_SRCS = tests/test_engine.cpp $(COMMON_SRCS)

.PHONY: all clean test server test-bin

all: server test-bin

server: $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) $(SERVER_SRCS) $(LDFLAGS) -o $(TARGET_SERVER)

test-bin: $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(LDFLAGS) -o $(TARGET_TEST)

test: test-bin
	./$(TARGET_TEST)

clean:
	$(RM) $(TARGET_SERVER) $(TARGET_TEST) *.aof *.tmp
