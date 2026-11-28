CC ?= gcc
AR ?= ar
CFLAGS ?= -O2 -Wall -Wextra -std=c11

BUILD_DIR ?= build
TARGET ?= libhashsetvalue.a
SOURCES = value.c crud.c map.c repr.c json.c
OBJECTS = $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPFILES = $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
