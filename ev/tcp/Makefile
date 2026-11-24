CC = gcc
CFLAGS = -Wall -Wextra -std=c11
CPPFLAGS = -I../..
LDLIBS = -lws2_32 -lmswsock

TARGET = ../../bin/tcp-connector.exe
SOURCES = loop.c task.c public.c objects.c utils.c init.c finish.c completion.c cb.c ../../hashmap/hashmap.c main.c
HEADERS = public.h models.h ../../hashmap/hashmap.h ../../value/value.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p ../../bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	$(RM) $(TARGET)
