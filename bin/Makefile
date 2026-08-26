CC = gcc
CFLAGS = -m64 -ffreestanding -fno-pic -fno-pie -nostdlib -nostartfiles -nodefaultlibs
LDFLAGS = -T linker.ld -no-pie -nostdlib -nostartfiles -nodefaultlibs

all: apps

apps: init shell

init: init.o
	$(CC) $(LDFLAGS) -o $@ $^

shell: shell_commands.o
	$(CC) $(LDFLAGS) -o $@ $^

init.o: init.c
	$(CC) $(CFLAGS) -I../kernel/include -I../kernel/arch/x86_64/include -I../kernel/arch/x86_64/include/arch -I../kernel/lib/libc/include -c $< -o $@

shell_commands.o: shell_commands.c
	$(CC) $(CFLAGS) -I../kernel/include -I../kernel/arch/x86_64/include -I../kernel/arch/x86_64/include/arch -I../kernel/lib/libc/include -c $< -o $@

clean:
	rm -f *.o init shell

.PHONY: all clean apps
