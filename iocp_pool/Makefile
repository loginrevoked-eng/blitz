CC = gcc
CFLAGS = -Wall -Wextra -std=c11
CPPFLAGS = -I..
LDLIBS = -lws2_32 -lmswsock

TARGET = iocp-pool-example.exe
SOURCES = iocp-pool.c main.c
HEADERS = iocp-pool.h ../core/models.h ../pool/cidr.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	$(RM) $(TARGET)
