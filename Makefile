CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
LDLIBS = -lws2_32 -liphlpapi
CPPFLAGS = -I. -Isrc -Iconfig -Ipool -Ilogging -Iutils

BIN_DIR = bin
BUILD_DIR = build
BOT = $(BIN_DIR)/bot.exe
DEMO = $(BIN_DIR)/demo.exe
ICMP = $(BIN_DIR)/icmp.exe

CORE_SRC = \
	cli/marquee.c \
	cli/marquee_progress.c \
	cli/progress.c \
	cli/spinner.c \
	logging/logging.c \
	utils/fs.c \
	pool/cidr.c \
	config/config.c \
	models.c \
	uibridge.c \
	probe/manager.c \
	probe/icmp.c \
	pool/picker.c \
	pool/bucket.c \
	pool/fiestel.c \
	cmdline/parser.c \
	cmdline/map.c \
	cmdline/panic.c \
	cmdline/value.c \
	str/str.c

CORE_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SRC))
BOT_OBJ = $(BUILD_DIR)/main.o $(CORE_OBJ)
DEMO_OBJ = $(BUILD_DIR)/demo.o $(CORE_OBJ)
ICMP_OBJ = $(BUILD_DIR)/main.o $(CORE_OBJ)

.PHONY: all bot demo icmp clean

all: bot

bot: $(BOT)

demo: $(DEMO)

icmp: $(ICMP)

$(BOT): $(BOT_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(DEMO): $(DEMO_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(ICMP): $(ICMP_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	rm -rf $(BUILD_DIR) $(BOT) $(DEMO) $(ICMP)
