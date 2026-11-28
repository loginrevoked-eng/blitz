CC = gcc
AR = ar

NPCAP_ROOT ?= C:/npcap-sdk-1.17
BUILD_DIR = build
TARGET = main.exe
PICKER_LIB = picker/libpicker.a
HASHSET_LIB = hashset/libsynscanner_hashset.a
HASHSET_VALUE_LIB = hashset/value/libhashsetvalue.a

CFLAGS ?= -O2 -Wall -Wextra -std=c11
CPPFLAGS += -I. -I$(NPCAP_ROOT)/Include -Dappendf=strappf
LDLIBS += -L$(NPCAP_ROOT)/Lib/x64 -lwpcap -liphlpapi -lws2_32 -lcrypto -lbcrypt

SOURCES = main.c logging.c route.c pkt.c str.c pcap_i.c list.c scan.c
OBJECTS = $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPFILES = $(OBJECTS:.o=.d)
PICKER_INPUTS = $(wildcard picker/*.c picker/*.h picker/crypto/*.c picker/crypto/*.h picker/value/*.c picker/value/*.h) picker/Makefile logging.c str.c
HASHSET_INPUTS = $(wildcard hashset/*.c hashset/*.h hashset/value/*.c hashset/value/*.h) hashset/Makefile hashset/value/Makefile

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(PICKER_LIB) $(HASHSET_LIB) $(HASHSET_VALUE_LIB)
	$(CC) $(OBJECTS) $(HASHSET_LIB) $(HASHSET_VALUE_LIB) $(PICKER_LIB) -o $@ $(LDLIBS)

$(PICKER_LIB): $(PICKER_INPUTS)
	$(MAKE) -C picker CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" \
		EXTRA_CPPFLAGS="$(CPPFLAGS) $(PICKER_CPPFLAGS)"

$(HASHSET_LIB): $(HASHSET_INPUTS) $(HASHSET_VALUE_LIB)
	$(MAKE) -C hashset CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)"

$(HASHSET_VALUE_LIB): $(HASHSET_INPUTS)
	$(MAKE) -C hashset/value CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)"

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	$(MAKE) -C picker clean
