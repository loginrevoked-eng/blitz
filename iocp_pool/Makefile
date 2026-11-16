CC = gcc
CFLAGS = -Wall -Wextra -std=c11
CPPFLAGS = -I..
LDLIBS = -lws2_32 -lmswsock

TARGET = ../bin/iocp-pool.exe
SOURCES = pool.c lists.c ctors.c ../hashmap/hashmap.c main.c
HEADERS = pool.h lists.h ctors.h ../hashmap/hashmap.h ../value/value.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p ../bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	$(RM) $(TARGET)
