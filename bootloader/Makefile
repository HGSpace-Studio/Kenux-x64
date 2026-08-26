CC = gcc
OBJCOPY = objcopy

CFLAGS = -Wall -O2 -fno-stack-protector -fno-builtin \
         -mno-red-zone -m64 -fshort-wchar -ffreestanding \
         -Iinclude -Wl,--subsystem,10 -e efi_main

TARGET = BOOTX64.EFI
SRC = bootx64.c

all: $(TARGET)

$(TARGET): $(SRC) include/efi.h include/fbc.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) -nostdlib -Wl,--disable-reloc-section
	$(OBJCOPY) -j .text -j .rdata -j .data -j .reloc --target=efi-app-x86_64 $(TARGET) $(TARGET).tmp
	move /Y $(TARGET).tmp $(TARGET)

clean:
	del /Q $(TARGET)
