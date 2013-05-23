SHAREDOPT = 
LIBDIR = /usr/lib
INCDIR = /usr/include
AR = ar
CC = gcc
INCS =  -Iinclude/ -I.
FLAGS = -Wall

all: obj/ecgi.o obj/ecgitk.o libecgi.a
	make -C html2h/

shared: libecgi.so
	cp libecgi.so /usr/lib

libecgi.a: obj/ecgi.o obj/ecgitk.o
	ar rs libecgi.a obj/ecgi.o obj/memfile.o obj/ecgitk.o

libecgi.so: obj/ecgi.o obj/ecgitk.o
	gcc $(SHAREDOPT) obj/ecgi.o obj/memfile.o obj/ecgitk.o -o libecgi.so

install:
	cp libecgi.a $(LIBDIR)
	cp ecgi.h $(INCDIR)
	cp include/memfile.h $(INCDIR)
	cp ecgitk.h $(INCDIR)
	make -C html2h/ install
	cp libecgi.so $(LIBDIR)

tests:	all
	$(CC) tests/test.c -o tests/test.cgi $(INCS) $(FLAGS) libecgi.a
	$(CC) tests/testload.c -o tests/testload libecgi.a $(INCS) $(FLAGS)

obj/ecgi.o: src/ecgi.c ecgi.h obj/memfile.o
	$(CC) -c src/ecgi.c $(INCS) $(FLAGS) -o obj/ecgi.o

obj/memfile.o:  src/memfile.c include/memfile.h
	$(CC) -o obj/memfile.o -c src/memfile.c $(INCS) $(FLAGS)

obj/ecgitk.o: src/ecgitk.c ecgitk.h
	$(CC) -c src/ecgitk.c $(INCS) $(FLAGS) -o obj/ecgitk.o

clean:
	rm -f obj/* *.a *.so -f tests/test.cgi tests/testload
	make -C html2h/ clean

zip: clean
	rm -f ../ecgi-0.6.2.zip
	(cd ..; zip -r ecgi-0.6.2.zip ecgi-0.6.2*)
