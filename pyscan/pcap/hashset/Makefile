CC ?= gcc
AR ?= ar
CFLAGS ?= -O2 -Wall -Wextra -std=c11

BUILD_DIR ?= build
TARGET ?= libsynscanner_hashset.a
VALUE_LIB = value/libhashsetvalue.a
OBJECTS = $(BUILD_DIR)/hashset.o
DEPFILES = $(OBJECTS:.o=.d)

.PHONY: all clean value

all: $(TARGET) $(VALUE_LIB)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

$(VALUE_LIB):
	$(MAKE) -C value CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)"

$(BUILD_DIR)/hashset.o: hashset.c hashset.h value/value.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPFILES)

value:
	$(MAKE) -C value CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	$(MAKE) -C value clean
