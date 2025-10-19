CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = flow

all: $(TARGET)

$(TARGET): flow.c
	$(CC) $(CFLAGS) -o $(TARGET) flow.c

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) filecount.flow doit

test2: $(TARGET)
	./$(TARGET) complicated.flow shenanigan

.PHONY: all clean test test2
