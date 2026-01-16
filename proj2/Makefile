CC = gcc
CFLAGS = -Wall -g
SRCS = main.c algo_fcfs.c algo_sjf.c algo_srt.c algo_rr.c algo_hpf.c
OBJS = $(SRCS:.c=.o)
TARGET = scheduler

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(OBJS) $(TARGET)