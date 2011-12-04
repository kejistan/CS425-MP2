all: chord_sys mp2_node

CC := gcc

CFLAGS   += -Wno-unused-parameter -Wextra -Wall -g -DDEBUG -std=c99
CPPFLAGS += -MMD -MP

sources := $(wildcard *.c)
objects := $(sources:.c=.o)
deps    := $(sources:.c=.d)

ifneq ($(MAKECMDGOALS), "clean")
-include $(deps)
endif

.PHONY : all
all : chord_sys mp2_node

chord_sys : LDFLAGS += -pthread
chord_sys : chord_sys.o
	$(LINK.c) -o $@ $^

mp2_node : node.o util.o map.o queue.o
	$(LINK.c) -o $@ $^

clean :
	-$(RM) chord_sys mp2_node $(objects) $(deps)
