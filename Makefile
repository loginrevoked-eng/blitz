CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -D_FORTIFY_SOURCE=2 -O1 -g -fno-omit-frame-pointer -fstack-protector-strong -fsanitize=undefined -fsanitize-undefined-trap-on-error
CPPFLAGS = -I. -Icore -Iconnector -Ipicker -Ipicker/crypto -Itelnet -Idata/value -Idata/hashmap -Idata/hashset -Idata/str -Icmdline -Ilogging -Iutils -Ilibs/libtelnet
LDLIBS = -lws2_32 -lmswsock -liphlpapi -lbcrypt -lm

BIN_DIR = bin
BUILD_DIR = build
CORE = $(BIN_DIR)/core.exe

CORE_SRC = \
	$(wildcard core/*.c) \
	$(wildcard connector/*.c) \
	$(wildcard picker/*.c picker/crypto/*.c) \
	logging/logging.c utils/fs.c \
	$(filter-out cmdline/main.c,$(wildcard cmdline/*.c)) \
	data/hashmap/hashmap.c data/hashset/hashset.c data/str/str.c \
	$(filter-out data/value/test.c data/value/value.c,$(wildcard data/value/*.c)) \
	telnet/telnet.c telnet/credit.c libs/libtelnet/libtelnet.c

CORE_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SRC))
CORE_DEP = $(CORE_OBJ:.o=.d)

.DEFAULT_GOAL := all
-include $(CORE_DEP)

.PHONY: all core telnet clean

all: core

core: $(CORE)

telnet:
	$(MAKE) -C telnet

$(CORE): $(CORE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(CORE_OBJ) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	rm -rf $(BUILD_DIR) $(CORE)
