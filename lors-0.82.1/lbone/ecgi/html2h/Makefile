CC = gcc
INCS =  -I../include/ -I.
FLAGS = -Wall

all: html2h

html2h: html2h.c html2h.h
	$(CC) html2h.c -o html2h $(INCS) $(FLAGS) ../obj/memfile.o

install: all
	cp html2h /usr/bin

clean:
	rm -f html2h