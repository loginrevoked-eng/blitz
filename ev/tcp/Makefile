CC = gcc
CFLAGS = -Wall -Wextra -std=c11
CPPFLAGS = -I../..
LDLIBS = -lws2_32 -lmswsock

TARGET = ../../bin/tcp-connector.exe
SOURCES = loop.c task.c public.c create.c await.c objects.c utils.c init.c finish.c completion.c callback.c ../../hashmap/hashmap.c ../../http/http.c main.c
HEADERS = public.h models.h ../../hashmap/hashmap.h ../../value/value.h ../../http/http.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p ../../bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	$(RM) $(TARGET)
