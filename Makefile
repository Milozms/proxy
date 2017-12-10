#
# Makefile for Proxy Lab 
#
# You may modify is file any way you like (except for the handin
# rule). Autolab will execute the command "make" on your specific 
# Makefile to build your proxy from sources.
#
CC = g++ --std=c++11
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy nameserver

nameserver: server.cpp DNSServer.cpp
	$(CC) -o nameserver server.cpp DNSServer.cpp

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

nproxy.o: nproxy.c csapp.h
	$(CC) $(CFLAGS) -c nproxy.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

nproxy: nproxy.o csapp.o
	$(CC) $(CFLAGS) nproxy.o csapp.o -o nproxy $(LDFLAGS)

proxy: proxy.o csapp.o mydns.cpp DNSClient.cpp
	$(CC) $(CFLAGS) proxy.o csapp.o mydns.cpp DNSClient.cpp -o proxy $(LDFLAGS)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

