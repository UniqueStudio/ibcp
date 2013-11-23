CC = gcc
OFED_PATH = /usr
DEFAULT_CFLAGS = -I${OFED_PATH}/include
DEFAULT_LDFLAGS = -L${OFED_PATH}/lib64 -L${OFED_PATH}/lib

CFLAGS += $(DEFAULT_CFLAGS) -g -Wall
LDFLAGS += $(DEFAULT_LDFLAGS) -libverbs -lpthread
OBJECTS = main.o sock.o file_linkedlist_creat.o readfile.o writefile.o ibcpnet.o
TARGETS = ibcp

all: $(TARGETS)

ibcp: main.o file_linkedlist_creat.o  readfile.o writefile.o ibcpnet.o sock.o
	$(CC) $^ -o $@ $(LDFLAGS) -lm

main.o:main.c config.h ibcpnet.o 
	$(CC) -c $(CFLAGS) -lm $<

ibcpnet.o:ibcpnet.c sock.o sock.c sock.h	
	$(CC) -c $(CFLAGS) $<

readfile.o:readfile.c
	$(CC) -c $(CFLAGS) -lm $< 
writefile.o:writefile.c
	$(CC) -c $(CFLAGS) -lm $< 

file_linkedlist_creat.o: file_linkedlist_creat.c  
	$(CC) -c $(CFLAGS) $<
sock.o: sock.c sock.h
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f $(OBJECTS) $(TARGETS)

