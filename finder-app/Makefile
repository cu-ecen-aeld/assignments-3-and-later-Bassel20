CC := $(CROSS_COMPILE)gcc

writer: writer.o

writer.o: writer.c
	$(CC) -c writer.c

clean:
	rm -f *.o writer