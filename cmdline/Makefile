CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE

TARGET = parser_example
SRC = main.c parser.c hashmap.c panic.c value.c str.c ctors.c mutex.c subcommand.c assert.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) $(TARGET).exe

.PHONY: all clean
