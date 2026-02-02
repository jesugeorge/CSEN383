CC = gcc
CFLAGS = -std=c11 -Wall -pthread
TARGET = proj3

all: $(TARGET)

$(TARGET): main.c proj3.h
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)
