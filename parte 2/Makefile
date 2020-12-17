CC = gcc

all: download

write: writenoncanonical.c
	$(CC) -w -o download clientTCP.c

clean:
	rm -f *.o *.d download
