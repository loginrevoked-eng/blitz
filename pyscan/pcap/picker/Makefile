CC = gcc
AR = ar

CFLAGS ?= -O2 -Wall -Wextra -std=c11
CPPFLAGS += -I. -I.. -Icrypto -Dappendf=strappf -DDebug=debug -DWarn=warn $(EXTRA_CPPFLAGS)

BUILD_DIR = build
TARGET = libpicker.a

PICKER_SOURCES = \
	bucket.c \
	cidr.c \
	deser.c \
	exclusion.c \
	hook.c \
	init.c \
	picker.c \
	resume.c \
	time.c \
	crypto/feistel.c \
	crypto/random.c \
	value/crud.c \
	value/json.c \
	value/map.c \
	value/repr.c \
	value/value.c

COMMON_SOURCES = ../logging.c ../str.c

PICKER_OBJECTS = $(addprefix $(BUILD_DIR)/,$(PICKER_SOURCES:.c=.o))
COMMON_OBJECTS = $(patsubst ../%.c,$(BUILD_DIR)/common/%.o,$(COMMON_SOURCES))
OBJECTS = $(PICKER_OBJECTS) $(COMMON_OBJECTS)
DEPFILES = $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/common/%.o: ../%.c
	mkdir -p $(dir $@)
	$(CC) $(filter-out -I. -Icrypto,$(CPPFLAGS)) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
