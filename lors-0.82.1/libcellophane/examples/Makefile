CC=gcc
CFLAGS= -I/home/vagrant/usr/include -L/home/vagrant/usr/lib -lcellophane  -g 

all:
	$(CC) -o client example_client.c $(CFLAGS)

run:
	./client 

clean:
	rm client *.o *~
