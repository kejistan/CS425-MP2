all: listener mp2_node

CC := gcc

CFLAGS   += -Wextra -Wall -g -DDEBUG -std=c99
CPPFLAGS += -MMD -MP

sources := $(wildcard *.c)
objects := $(sources:.c=.o)
deps    := $(sources:.c=.d)

ifneq ($(MAKECMDGOALS), "clean")
-include $(deps)
endif

.PHONY : all
all : listener mp2_node

listener : LDFLAGS += -pthread
listener : listener.o
	$(LINK.c) -o $@ $^

mp2_node : node.o util.o map.o queue.o
	$(LINK.c) -o $@ $^

clean :
	-$(RM) listener mp2_node $(objects) $(deps)
