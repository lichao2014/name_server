CC := gcc
AR := ar
CFLAGS := -g3 -O0 -Wall -Werror

TARGETS := server client slice_test list_test libcomm.a

all:$(TARGETS)

server:server.o libcomm.a
	$(CC) $< -o $@ -lcomm -L.

client:client.o libcomm.a
	$(CC) $< -o $@ -lcomm -L.

libcomm.a:slice.o net.o
	$(AR) rcs $@ $^

list_test:list.h

slice_test:slice.o slice.h

SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=%.o)

$(OBJS):%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY:
	clean
	all

clean:
	-rm $(OBJS) $(TARGETS)
