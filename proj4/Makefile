CC = gcc
CFLAGS = -std=c11 -Wall
TARGET = proj4

.PHONY: all clean

all: clean $(TARGET)

$(TARGET): main.c proj4.h
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)
