CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
LDLIBS = -lws2_32 -liphlpapi -lbcrypt
CPPFLAGS = -I. -Ilogging -Iutils -Ivalue

BIN_DIR = bin
BUILD_DIR = build
CORE = $(BIN_DIR)/core.exe
CORE_LUA_LIB = lua/lib/liblua.a

CORE_SRC = \
	$(wildcard core/*.c core/ui/*.c core/scripting/*.c) \
	logging/logging.c \
	utils/fs.c \
	$(wildcard pool/*.c crypto/*.c) \
	$(filter-out value/test.c value/value.c,$(wildcard value/*.c)) \
	str/str.c \
	cmdline/parser.c \
	cmdline/hashmap.c \
	cmdline/str.c \
	cmdline/ctors.c \
	cmdline/mutex.c \
	cmdline/subcommand.c \
	cmdline/assert.c \
	cmdline/panic.c \
	cmdline/value.c
CORE_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SRC))

.PHONY: all core clean

all: core

core: $(CORE)

$(CORE): $(CORE_OBJ) $(CORE_LUA_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(CORE_OBJ) $(CORE_LUA_LIB) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	rm -rf $(BUILD_DIR) $(CORE)
