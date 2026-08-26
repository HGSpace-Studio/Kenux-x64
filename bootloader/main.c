#include <stddef.h>
#include "efi.h"
#include "graphics.h"
#include "progress.h"
#include "kernel.h"
#include "serial.h"

void delay(uint64_t iterations) {
    for (volatile uint64_t i = 0; i < iterations; i++) {
        __asm__ volatile ("nop");
    }
}

void draw_gradient_background(screen_t *screen) {
    for (uint32_t y = 0; y < screen->height; y++) {
        uint32_t t = (y * 256) / screen->height;
        uint8_t r = (uint8_t)(20 + (t * 20) / 256);
        uint8_t g = (uint8_t)(20 + (t * 20) / 256);
        uint8_t b = (uint8_t)(40 + (t * 40) / 256);
        draw_rect(screen, 0, y, screen->width, 1, r, g, b);
    }
}

void draw_logo(screen_t *screen) {
    uint32_t cx = screen->width / 2;
    uint32_t cy = screen->height / 3;
    const char *logo = "KEGD";
    uint32_t x = cx - 100;
    uint32_t y = cy - 40;
    
    uint8_t colors[4][3] = {
        {0, 120, 215},
        {0, 150, 136},
        {0, 180, 215},
        {76, 175, 80}
    };
    
    for (int i = 0; logo[i]; i++) {
        uint8_t r = colors[i][0];
        uint8_t g = colors[i][1];
        uint8_t b = colors[i][2];
        draw_string(screen, x + i * 50, y, &logo[i], r, g, b);
        
        for (int j = 0; j < 5; j++) {
            draw_rect(screen, x + i * 50, y + 20 + j * 2, 40, 1, r, g, b);
        }
    }
}

void draw_status_bar(screen_t *screen) {
    uint32_t bar_height = 50;
    uint32_t y = screen->height - bar_height;
    draw_rect(screen, 0, y, screen->width, bar_height, 15, 15, 15);
    draw_rect(screen, 0, y, screen->width, 2, 0, 120, 215);
}

static const int16_t sin_table[256] = {
    0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92,
    98, 104, 109, 115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177,
    181, 186, 190, 194, 198, 202, 206, 210, 213, 217, 220, 223, 226, 229, 232, 235,
    237, 240, 242, 244, 246, 248, 250, 251, 253, 254, 255, 255, 256, 256, 256, 256,
    256, 256, 256, 255, 255, 254, 253, 251, 250, 248, 246, 244, 242, 240, 237, 235,
    232, 229, 226, 223, 220, 217, 213, 210, 206, 202, 198, 194, 190, 186, 181, 177,
    172, 167, 162, 157, 152, 147, 142, 137, 132, 126, 121, 115, 109, 104, 98, 92,
    86, 80, 74, 68, 62, 56, 50, 44, 38, 31, 25, 19, 13, 6, 0, -6,
    -13, -19, -25, -31, -38, -44, -50, -56, -62, -68, -74, -80, -86, -92, -98, -104,
    -109, -115, -121, -126, -132, -137, -142, -147, -152, -157, -162, -167, -172, -177,
    -181, -186, -190, -194, -198, -202, -206, -210, -213, -217, -220, -223, -226, -229,
    -232, -235, -237, -240, -242, -244, -246, -248, -250, -251, -253, -254, -255, -255,
    -256, -256, -256, -256, -256, -256, -256, -255, -255, -254, -253, -251, -250, -248,
    -246, -244, -242, -240, -237, -235, -232, -229, -226, -223, -220, -217, -213, -210,
    -206, -202, -198, -194, -190, -186, -181, -177, -172, -167, -162, -157, -152, -147,
    -142, -137, -132, -126, -121, -115, -109, -104, -98, -92, -86, -80, -74, -68, -62,
    -56, -50, -44, -38, -31, -25, -19, -13, -6
};

static inline int16_t fixed_sin(uint8_t angle) {
    return sin_table[angle & 0xFF];
}

static inline int16_t fixed_cos(uint8_t angle) {
    return sin_table[(angle + 64) & 0xFF];
}

void draw_loading_animation(screen_t *screen, uint32_t cx, uint32_t cy, uint32_t radius, uint8_t angle) {
    uint32_t num_dots = 8;
    for (uint32_t i = 0; i < num_dots; i++) {
        uint8_t a = angle + (i * 32);
        int32_t dx = ((int32_t)radius * fixed_cos(a)) / 256;
        int32_t dy = ((int32_t)radius * fixed_sin(a)) / 256;
        uint32_t x = (uint32_t)((int32_t)cx + dx);
        uint32_t y = (uint32_t)((int32_t)cy + dy);
        
        uint32_t intensity = (256 * (num_dots - i)) / num_dots;
        uint8_t r = 0;
        uint8_t g = (uint8_t)((120 * intensity) / 256);
        uint8_t b = (uint8_t)((215 * intensity) / 256);
        
        draw_filled_circle(screen, x, y, 5, r, g, b);
    }
}

static efi_status_t leave_boot_services(efi_handle_t image_handle, efi_system_table_t *system_table) {
    efi_boot_services_t *bs = system_table->boot_services;
    efi_get_memory_map_t get_memory_map = (efi_get_memory_map_t)bs->get_memory_map;
    efi_allocate_pool_t allocate_pool = (efi_allocate_pool_t)bs->allocate_pool;
    efi_free_pool_t free_pool = (efi_free_pool_t)bs->free_pool;
    efi_exit_boot_services_t exit_boot_services = (efi_exit_boot_services_t)bs->exit_boot_services;
    efi_uintn_t map_size = 0;
    efi_uintn_t map_key = 0;
    efi_uintn_t descriptor_size = 0;
    uint32_t descriptor_version = 0;
    efi_status_t status = get_memory_map(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    map_size += descriptor_size * 8;
    efi_memory_descriptor_t *memory_map = NULL;
    status = allocate_pool(EFI_LOADER_DATA, map_size, (void **)&memory_map);
    if (status != EFI_SUCCESS) {
        return status;
    }

    status = get_memory_map(&map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);
    if (status == EFI_SUCCESS) {
        status = exit_boot_services(image_handle, map_key);
    }

    if (status != EFI_SUCCESS) {
        free_pool(memory_map);
    }
    return status;
}

efi_status_t efi_main(efi_handle_t image_handle, efi_system_table_t *system_table) {
    screen_t screen;
    progress_bar_t progress;
    uint8_t anim_angle = 0;

    serial_init();
    serial_write("Kenux bootloader: entry\n");
    
    init_graphics(system_table, &screen);
    serial_write("Kenux bootloader: graphics initialized\n");
    serial_write("Kenux bootloader: framebuffer addr=");
    serial_write_hex64(screen.frame_buffer);
    serial_write(" size=");
    serial_write_hex64(screen.buffer_size);
    serial_write(" width=");
    serial_write_hex64(screen.width);
    serial_write(" height=");
    serial_write_hex64(screen.height);
    serial_write("\n");
    draw_gradient_background(&screen);
    draw_logo(&screen);
    draw_status_bar(&screen);
    
    uint32_t pb_width = 400;
    uint32_t pb_height = 20;
    uint32_t pb_x = (screen.width - pb_width) / 2;
    uint32_t pb_y = screen.height - 150;
    init_progress_bar(&progress, pb_x, pb_y, pb_width, pb_height);
    draw_progress_bar(&screen, &progress);
    
    uint32_t anim_cx = screen.width / 2;
    uint32_t anim_cy = pb_y - 50;
    
    const char *boot_steps[] = {
        "Initializing...",
        "Loading modules...",
        "Starting OS..."
    };
    
    int num_steps = sizeof(boot_steps) / sizeof(boot_steps[0]);
    
    for (int i = 0; i < num_steps; i++) {
        draw_string(&screen, 20, 20, boot_steps[i], 255, 255, 255);
        
        for (int j = 0; j <= 100; j += 20) {
            set_progress(&progress, j);
            draw_progress_bar(&screen, &progress);
            
            anim_angle += 16;
            draw_loading_animation(&screen, anim_cx, anim_cy, 30, anim_angle);
            
            delay(50000);
        }
        
        delay(50000);
    }
    
    set_progress(&progress, 100);
    draw_progress_bar(&screen, &progress);
    draw_string(&screen, screen.width/2 - 50, screen.height/2, "BOOT COMPLETE!", 0, 255, 0);
    
    delay(100000);
    
    void *kernel_entry = NULL;
    void *mb_info = NULL;
    
    draw_string(&screen, 20, 60, "Loading kernel...", 255, 255, 255);
    serial_write("Kenux bootloader: loading kernel\n");
    
    efi_status_t status = load_kernel(image_handle, system_table, "\\boot\\kernel.elf", &kernel_entry, &mb_info, screen.frame_buffer, screen.buffer_size);
    
    if (status == EFI_SUCCESS && kernel_entry != NULL) {
        multiboot_info_t *info = (multiboot_info_t*)mb_info;
        info->flags |= 0x00001000;
        info->framebuffer_addr = screen.frame_buffer;
        info->framebuffer_pitch = screen.pixels_per_scanline * 4;
        info->framebuffer_width = screen.width;
        info->framebuffer_height = screen.height;
        info->framebuffer_bpp = 32;
        info->framebuffer_type = (uint8_t)screen.pixel_format;

        serial_write("Kenux bootloader: kernel loaded\n");
        draw_string(&screen, 20, 80, "Kernel loaded!", 0, 255, 0);
        
        draw_string(&screen, 20, 100, "Jumping to kernel...", 255, 255, 255);
        delay(100000);
        
        status = leave_boot_services(image_handle, system_table);
        if (status != EFI_SUCCESS) {
            serial_write("Kenux bootloader: ExitBootServices failed\n");
            draw_string(&screen, 20, 120, "ExitBootServices failed!", 255, 0, 0);
            while(1) {
                __asm__ volatile ("hlt");
            }
        }
        
        serial_write("Kenux bootloader: jumping to kernel\n");
        // 跳转到内核
        jump_to_kernel((kernel_entry_t)kernel_entry, 0x2BADB002, (multiboot_info_t*)mb_info);
    } else {
        serial_write("Kenux bootloader: kernel load failed\n");
        draw_string(&screen, 20, 80, "No kernel found!", 255, 0, 0);
        
        while(1) {
            __asm__ volatile ("hlt");
        }
    }
    
    return EFI_SUCCESS;
}
