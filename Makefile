CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -D_FORTIFY_SOURCE=2 -O1 -g -fno-omit-frame-pointer -fstack-protector-strong -fsanitize=undefined -fsanitize-undefined-trap-on-error
LDLIBS = -lws2_32 -liphlpapi -lbcrypt
CPPFLAGS = -I. -Ilogging -Iutils -Ivalue

BIN_DIR = bin
BUILD_DIR = build
CORE = $(BIN_DIR)/core.exe
IO_ASYNC_TEST = $(BIN_DIR)/io-async-test.exe
IO_ASYNC_MIN = $(BIN_DIR)/io-async-min.exe
IOCP_POOL = $(BIN_DIR)/iocp-pool.exe
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

.PHONY: all core io-async-test io-async-min iocp-pool clean

all: core

core: $(CORE)

io-async-test: $(IO_ASYNC_TEST)

io-async-min: $(IO_ASYNC_MIN)

iocp-pool: $(IOCP_POOL)

$(IO_ASYNC_TEST): io-async.c io-async-test.c io-async.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ io-async.c io-async-test.c -lws2_32 -lmswsock

$(IO_ASYNC_MIN): io-async-min.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ io-async-min.c -lws2_32 -lmswsock

$(IOCP_POOL): iocp_pool/pool.c iocp_pool/pool.h iocp_pool/lists.c iocp_pool/lists.h iocp_pool/ctors.c iocp_pool/ctors.h iocp_pool/main.c hashmap/hashmap.c hashmap/hashmap.h value/value.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ iocp_pool/pool.c iocp_pool/lists.c iocp_pool/ctors.c hashmap/hashmap.c iocp_pool/main.c -lws2_32 -lmswsock

$(CORE): $(CORE_OBJ) $(CORE_LUA_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(CORE_OBJ) $(CORE_LUA_LIB) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	rm -rf $(BUILD_DIR) $(CORE) $(IO_ASYNC_TEST) $(IO_ASYNC_MIN) $(IOCP_POOL)
