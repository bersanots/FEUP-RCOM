CC = gcc

all: write read

write: writenoncanonical.c
	$(CC) -w -o write writenoncanonical.c SU_frame.c

read: noncanonical.c
	$(CC) -w -o read noncanonical.c SU_frame.c

clean:
	rm -f *.o *.d read write
