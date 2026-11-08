CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
LDLIBS = -lws2_32 -liphlpapi -lbcrypt
CPPFLAGS = -I. -Isrc -Iconfig -Ibanners -iquote pool -iquote value -Ilogging -Iutils

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
	crypto/random.c \
	pool/time.c \
	pool/cidr.c \
	config/config.c \
	models.c \
	uibridge.c \
	probe/manager.c \
	icmp.c \
	pool/init.c \
	pool/hook.c \
	pool/picker.c \
	pool/resume.c \
	pool/deser.c \
	pool/bucket.c \
	crypto/feistel.c \
	cmdline/parser.c \
	cmdline/map.c \
	cmdline/panic.c \
	cmdline/value.c \
	str/str.c

CORE_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SRC))
BOT_OBJ = $(BUILD_DIR)/main.o $(CORE_OBJ)
DEMO_SRC = \
	demo.c \
	udp.c \
	logging/logging.c \
	utils/fs.c \
	crypto/random.c \
	pool/time.c \
	pool/cidr.c \
	pool/init.c \
	pool/hook.c \
	pool/picker.c \
	pool/resume.c \
	pool/deser.c \
	pool/bucket.c \
	crypto/feistel.c \
	value/crud.c \
	value/map.c \
	value/json.c \
	str/str.c
DEMO_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(DEMO_SRC))
ICMP_OBJ = $(BUILD_DIR)/main.o $(CORE_OBJ)

.PHONY: all bot demo icmp clean

all: bot

bot: $(BOT)

PROBE_SRC = probe/main.c probe/probe.c probe/config.c probe/init.c probe/repr.c probe/icmp.c probe/tcp.c probe/udp.c probe/io/signal.c \
	banners/banners.c banners/http.c banners/ftp.c banners/rtsp.c banners/lookup.c \
	pool/time.c pool/cidr.c pool/exclusion.c pool/init.c pool/hook.c pool/picker.c \
	crypto/feistel.c crypto/random.c hashset/hashset.c \
	tui/marquee.c cmdline/parser.c cmdline/map.c cmdline/panic.c cmdline/value.c \
	value/crud.c value/map.c value/json.c pool/resume.c pool/deser.c pool/bucket.c \
	utils/fs.c str/str.c logging/logging.c
$(BOT): $(PROBE_SRC) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ -lws2_32 -liphlpapi -lbcrypt

demo: $(DEMO)

icmp: $(ICMP)

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
