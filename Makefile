CFLAGS = -std=gnu99 -g
LDFLAGS = -lpthread
CC = gcc

dummy-acc: mem-dummy.o access.o
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	-rm -f *.o
