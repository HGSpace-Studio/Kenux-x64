#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <systemd.h>
#include <arch/vga.h>

void init_systemd_services(void)
{
    char* services[] = {
        "container-os.service",
        "shell.service",
        NULL
    };
    
    for (int i = 0; services[i]; i++) {
        systemd_start_service(services[i]);
    }
}

void systemd_main(void)
{
    vga_write("systemd: Starting system manager...\n", 35);
    
    systemd_init();
    
    vga_write("systemd: Loading units...\n", 27);
    
    systemd_run();
}