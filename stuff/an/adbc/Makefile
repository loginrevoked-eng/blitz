CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11 -O2
SOURCES = main.c client.c socket.c log.c

ifeq ($(OS),Windows_NT)
TARGET = adbc.exe
LDLIBS += -lws2_32
else
TARGET = adbc
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) adbc.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	$(RM) $(TARGET)
