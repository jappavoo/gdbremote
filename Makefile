CFLAGS=-g 

gdbRemote: main.o gdbremote.o
	gcc $(LDFLAGS) $? -o $@

main.o: main.c gdbremote.h
	gcc $(CFLAGS) $? -o $@

gdbremote.o: gdbremote.c gdbremote.h
	gcc $(CFLAGS) $? -o $@
