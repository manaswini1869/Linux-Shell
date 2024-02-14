BIN_DIR = ./usr/bin

CC = gcc 
CFLAGS = -Wall -Werror -pedantic -std=gnu18
LOGIN = manaswini-09
SUBMITPATH = ~cs537-1/handin/manaswini-09/P3


SRCS = wsh.c
HDRS = wsh.h
OBJS = $(SRCS:.c=.o)
executables = wsh


.PHONY: all
all: wsh

wsh: $(OBJS)
	$(CC) $(CFLAGS) -o $(executables) $^

run: wsh
	./$(executables)

pack: README.md 
	tar -cvzf $(LOGIN).tar.gz $(SRCS) $(HDRS) Makefile README.md

submit: pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)

clean:
	rm -f $(executables) $(OBJS) $(LOGIN).tar.gz 

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@ 