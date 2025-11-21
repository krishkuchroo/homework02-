CC = gcc
CFLAGS = -pthread -Wall -O2

all: parallel_hashtable parallel_mutex parallel_spin parallel_mutex_opt

parallel_hashtable: parallel_hashtable.c
	$(CC) $(CFLAGS) parallel_hashtable.c -o parallel_hashtable

parallel_mutex: parallel_mutex.c
	$(CC) $(CFLAGS) parallel_mutex.c -o parallel_mutex

parallel_spin: parallel_spin.c
	$(CC) $(CFLAGS) parallel_spin.c -o parallel_spin

parallel_mutex_opt: parallel_mutex_opt.c
	$(CC) $(CFLAGS) parallel_mutex_opt.c -o parallel_mutex_opt

clean:
	rm -f parallel_hashtable parallel_mutex parallel_spin parallel_mutex_opt

.PHONY: all clean
