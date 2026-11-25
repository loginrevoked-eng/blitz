CC = gcc
CFLAGS = -Wall -Wextra -std=c11
CPPFLAGS = -I..
LDLIBS = -lws2_32 -lmswsock

TARGET = ../bin/tcp-connector.exe
MAIN_TARGET = main.exe
SOURCES = locks.c condvars.c builder.c loop.c submit.c dispatch.c task.c queue.c socket.c create.c await.c objects.c utils.c init.c finish.c completion.c callback.c ../hashmap/hashmap.c ../hashset/hashset.c main.c
HEADERS = public.h models.h internal.h ../hashmap/hashmap.h ../hashset/hashset.h ../value/value.h

.PHONY: all clean

all: $(TARGET)

$(MAIN_TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p ../bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	$(RM) $(TARGET) $(MAIN_TARGET)
