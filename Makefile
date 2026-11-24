CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -D_FORTIFY_SOURCE=2 -O1 -g -fno-omit-frame-pointer -fstack-protector-strong -fsanitize=undefined -fsanitize-undefined-trap-on-error
LDLIBS = -lws2_32 -liphlpapi -lbcrypt
CPPFLAGS = -I. -Ilogging -Iutils -Ivalue

BIN_DIR = bin
BUILD_DIR = build
CORE = $(BIN_DIR)/core.exe
IO_ASYNC_TEST = $(BIN_DIR)/io-async-test.exe
IO_ASYNC_MIN = $(BIN_DIR)/io-async-min.exe
TCP_CONNECTOR = $(BIN_DIR)/tcp-connector.exe
CORE_LUA_LIB = lua/lib/liblua.a

CORE_SRC = \
	$(wildcard core/*.c core/ui/*.c core/scripting/*.c) \
	ev/tcp/loop.c ev/tcp/task.c ev/tcp/public.c ev/tcp/create.c ev/tcp/await.c \
	ev/tcp/objects.c ev/tcp/utils.c ev/tcp/init.c \
	ev/tcp/finish.c ev/tcp/completion.c ev/tcp/callback.c \
	hashmap/hashmap.c \
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
CORE_DEP = $(CORE_OBJ:.o=.d)

.DEFAULT_GOAL := all
-include $(CORE_DEP)

.PHONY: all core io-async-test io-async-min tcp-connector clean

all: core

core: $(CORE)

io-async-test: $(IO_ASYNC_TEST)

io-async-min: $(IO_ASYNC_MIN)

tcp-connector: $(TCP_CONNECTOR)

$(IO_ASYNC_TEST): io-async.c io-async-test.c io-async.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ io-async.c io-async-test.c -lws2_32 -lmswsock

$(IO_ASYNC_MIN): io-async-min.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ io-async-min.c -lws2_32 -lmswsock

TCP_CONNECTOR_SRC = ev/tcp/loop.c ev/tcp/task.c ev/tcp/public.c ev/tcp/create.c ev/tcp/await.c ev/tcp/objects.c ev/tcp/utils.c ev/tcp/init.c ev/tcp/finish.c ev/tcp/completion.c ev/tcp/callback.c ev/tcp/main.c
TCP_CONNECTOR_HEADERS = ev/tcp/public.h ev/tcp/models.h

$(TCP_CONNECTOR): $(TCP_CONNECTOR_SRC) $(TCP_CONNECTOR_HEADERS) hashmap/hashmap.c hashmap/hashmap.h value/value.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(TCP_CONNECTOR_SRC) hashmap/hashmap.c -lws2_32 -lmswsock

$(CORE): $(CORE_OBJ) $(CORE_LUA_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(CORE_OBJ) $(CORE_LUA_LIB) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	rm -rf $(BUILD_DIR) $(CORE) $(IO_ASYNC_TEST) $(IO_ASYNC_MIN) $(TCP_CONNECTOR)
