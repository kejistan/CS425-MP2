all: listener mp2_node

CC := gcc
CFLAGS := -Wextra -Wall -g -DDEBUG

listener: listener.c
	$(CC) $(CFLAGS) -pthread -o $@ listener.c

mp2_node: node.c
	$(CC) $(CFLAGS) -o $@ node.c

clean:
	-rm -f listener mp2_node *.o

