CFLAGS=-g -c

GDBSRC=gdb-12.1

.PHONEY: distclean clean

gdbRemote: main.o gdbremote.o gdbops.o gdbremote_x86_64_xml.o gdbremote_i386_xml.o
	gcc $(LDFLAGS) $^ -o $@

main.o: main.c gdbremote.h
	gcc $(CFLAGS) $< -o $@

gdbremote.o: gdbremote.c gdbremote.h $(GDBSRC)/include/gdb/signals.h
	gcc $(CFLAGS)  -I $(GDBSRC)/include $< -o $@

gdbops.o: gdbops.c
	gcc $(CFLAGS) -I $(GDBSRC)/include $< -o $@

gdbremote_x86_64_xml.o: gdbremote.x86-64.xml
	objcopy --input binary -B i386 --output elf64-x86-64  $< $@

gdbremote_i386_xml.o: gdbremote.i386.xml
	objcopy --input binary -B i386 --output elf64-x86-64 $< $@


$(GDBSRC)/include/gdb/signals.h: $(GDBSRC).tar.gz
	tar -zxf $<
# force timestamp to be locally uptodate
	touch $@

$(GDBSRC).tar.gz:
	wget http://ftp.gnu.org/gnu/gdb/$@

clean:
	rm -rf $(wildcard *.o gdbremote)

distclean:
	rm -rf $(wildcard $(GDBSRC).tar.gz $(GDBSRC) )
