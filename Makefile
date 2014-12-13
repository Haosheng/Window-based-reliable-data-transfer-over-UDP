CC=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = client.o server.o
FILES = client server
all:$(FILES)
.PHONY:all

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

server: server.o 
	$(CC) -o $@ $^ $(CFLAGS)
.PHONY:clean
clean:
	-rm $(FILES) $(OBJ)

