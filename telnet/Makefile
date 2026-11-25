CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -D_FORTIFY_SOURCE=2 -O1 -g -fno-omit-frame-pointer -fstack-protector-strong -fsanitize=undefined -fsanitize-undefined-trap-on-error
CPPFLAGS = -I.. -I../connector -I../picker -I../data/hashmap -I../data/hashset -I../data/value -I../data/str -I../libs/libtelnet
LDLIBS = -lws2_32 -lmswsock -lm

BIN_DIR = ../bin
BUILD_DIR = ../build/telnet
TARGET = $(BIN_DIR)/telnet.exe
TELNET_SRC = telnet.c main.c credit.c ../libs/libtelnet/libtelnet.c
CONNECTOR_SRC = $(wildcard ../connector/*.c)
DATA_SRC = ../data/hashmap/hashmap.c ../data/hashset/hashset.c \
	$(filter-out ../data/value/test.c ../data/value/value.c,$(wildcard ../data/value/*.c))
EXTERNAL_SRC = ../libs/libtelnet/libtelnet.c ../picker/cidr.c ../data/str/str.c $(CONNECTOR_SRC) $(DATA_SRC)
TELNET_OBJ = $(BUILD_DIR)/telnet.o $(BUILD_DIR)/main.o $(BUILD_DIR)/credit.o
EXTERNAL_OBJ = $(patsubst ../%.c,$(BUILD_DIR)/%.o,$(EXTERNAL_SRC))
OBJECTS = $(TELNET_OBJ) $(EXTERNAL_OBJ)
DEPS = $(OBJECTS:.o=.d)

.DEFAULT_GOAL := all
-include $(DEPS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p "$(BIN_DIR)"
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR)/telnet.o: telnet.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BUILD_DIR)/main.o: main.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BUILD_DIR)/credit.o: credit.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BUILD_DIR)/%.o: ../%.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
