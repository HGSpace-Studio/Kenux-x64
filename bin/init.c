#include <stdio.h>
#include <string.h>
#include <memory.h>

void init_main(void)
{
    char* vga = (char*)0xb8000;
    const char* msg = "Welcome to Kenux User Space!\n";
    int i = 0;
    int pos = 0;
    
    while (msg[i] != '\0') {
        if (msg[i] == '\n') {
            pos = (pos / 160 + 1) * 160;
        } else {
            vga[pos * 2] = msg[i];
            vga[pos * 2 + 1] = 0x07;
            pos++;
        }
        i++;
    }
    
    const char* msg2 = "User mode initialized successfully.\n";
    i = 0;
    while (msg2[i] != '\0') {
        if (msg2[i] == '\n') {
            pos = (pos / 160 + 1) * 160;
        } else {
            vga[pos * 2] = msg2[i];
            vga[pos * 2 + 1] = 0x07;
            pos++;
        }
        i++;
    }
    
    char buffer[64] = "PID: 1\n";
    i = 0;
    while (buffer[i] != '\0') {
        if (buffer[i] == '\n') {
            pos = (pos / 160 + 1) * 160;
        } else {
            vga[pos * 2] = buffer[i];
            vga[pos * 2 + 1] = 0x07;
            pos++;
        }
        i++;
    }
    
    while (1) {
        const char* prompt = "init$ ";
        i = 0;
        while (prompt[i] != '\0') {
            if (prompt[i] == '\n') {
                pos = (pos / 160 + 1) * 160;
            } else {
                vga[pos * 2] = prompt[i];
                vga[pos * 2 + 1] = 0x07;
                pos++;
            }
            i++;
        }
        
        const char* echo_msg = "echo Hello from user space\n";
        i = 0;
        while (echo_msg[i] != '\0') {
            if (echo_msg[i] == '\n') {
                pos = (pos / 160 + 1) * 160;
            } else {
                vga[pos * 2] = echo_msg[i];
                vga[pos * 2 + 1] = 0x07;
                pos++;
            }
            i++;
        }
        
        i = 0;
        while (prompt[i] != '\0') {
            if (prompt[i] == '\n') {
                pos = (pos / 160 + 1) * 160;
            } else {
                vga[pos * 2] = prompt[i];
                vga[pos * 2 + 1] = 0x07;
                pos++;
            }
            i++;
        }
    }
}
