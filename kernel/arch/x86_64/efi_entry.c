#include <arch/efi.h>
#include <arch/vga.h>
#include <arch/memory.h>
#include <arch/framebuffer.h>
#include <string.h>

void kernel_main(void);

static efi_handle_t efi_image_handle = NULL;

static void efi_print_string(const char* str)
{
    if (!str) return;
    while (*str) {
        vga_putc(*str++);
    }
}

static void efi_print_hex(uint64_t value)
{
    const char* hex = "0123456789ABCDEF";
    char buf[20];
    int i = 18;
    buf[19] = 0;
    if (value == 0) {
        buf[i--] = '0';
    } else {
        while (value > 0 && i >= 0) {
            buf[i--] = hex[value & 0xF];
            value >>= 4;
        }
    }
    buf[i--] = 'x';
    buf[i] = '0';
    efi_print_string(&buf[i]);
}

static void efi_print_dec(uint64_t value)
{
    char buf[24];
    int i = 22;
    buf[23] = 0;
    if (value == 0) {
        buf[i--] = '0';
    } else {
        while (value > 0 && i >= 0) {
            buf[i--] = '0' + (value % 10);
            value /= 10;
        }
    }
    efi_print_string(&buf[i + 1]);
}

static efi_status_t efi_setup_framebuffer(void)
{
    efi_status_t status;
    efi_graphics_output_protocol_t* gop = NULL;
    uint32_t best_mode = 0;
    uint32_t best_score = 0;

    status = efi_gop_get_framebuffer(efi_image_handle, &gop);
    if (status != EFI_SUCCESS || !gop) {
        return status;
    }

    if (gop->mode && gop->mode->info) {
        efi_gop_mode = gop->mode;
    }

    for (uint32_t i = 0; i < gop->mode->max_mode; i++) {
        efi_graphics_output_mode_information_t* info = NULL;
        uint64_t size_of_info = 0;
        uint64_t (*query_mode)(efi_graphics_output_protocol_t*, uint32_t, uint64_t*, efi_graphics_output_mode_information_t**);
        query_mode = (uint64_t (*)(efi_graphics_output_protocol_t*, uint32_t, uint64_t*, efi_graphics_output_mode_information_t**))gop->query_mode;
        status = query_mode(gop, i, &size_of_info, &info);
        if (status != EFI_SUCCESS || !info) continue;

        uint32_t score = info->horizontal_resolution * info->vertical_resolution;
        if (info->pixel_format == pixel_blue_green_red_reserved_8bit_per_color ||
            info->pixel_format == pixel_red_green_blue_reserved_8bit_per_color) {
            score += 0x10000000;
        }

        if (score > best_score) {
            best_score = score;
            best_mode = i;
        }
    }

    if (best_mode != gop->mode->mode) {
        status = efi_gop_set_mode(gop, best_mode);
        if (status != EFI_SUCCESS) {
            status = efi_gop_set_mode(gop, 0);
        }
    }

    if (gop->mode) {
        efi_gop_mode = gop->mode;
    }

    return EFI_SUCCESS;
}

static void efi_parse_memory_map(void)
{
    if (!efi_memory_map || efi_descriptor_size == 0) return;

    uint64_t count = efi_memory_map_size / efi_descriptor_size;
    uint64_t total_pages = 0;
    uint64_t usable_pages = 0;

    for (uint64_t i = 0; i < count; i++) {
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint8_t*)efi_memory_map + i * efi_descriptor_size);
        total_pages += desc->number_of_pages;
        if (desc->type == EFI_CONVENTIONAL_MEMORY) {
            usable_pages += desc->number_of_pages;
        }
    }

    efi_print_string("Memory map entries: ");
    efi_print_dec(count);
    efi_print_string("\n");
    efi_print_string("Total pages: ");
    efi_print_dec(total_pages * 4096 / 1024 / 1024);
    efi_print_string(" MB\n");
    efi_print_string("Usable pages: ");
    efi_print_dec(usable_pages * 4096 / 1024 / 1024);
    efi_print_string(" MB\n");
}

static void efi_transition_to_kernel(void)
{
    fb_info_t* fb = fb_get_info();
    if (fb && efi_gop_mode && efi_gop_mode->frame_buffer_base) {
        fb->phys_addr = efi_gop_mode->frame_buffer_base;
        fb->width = efi_gop_mode->info->horizontal_resolution;
        fb->height = efi_gop_mode->info->vertical_resolution;
        fb->bpp = 32;
        fb->bytes_per_line = efi_gop_mode->info->pixels_per_scan_line * 4;
        fb->total_size = efi_gop_mode->frame_buffer_size;

        if (efi_gop_mode->info->pixel_format == pixel_blue_green_red_reserved_8bit_per_color) {
            fb->pixel_format = FB_PIXEL_FORMAT_BGRA;
        } else if (efi_gop_mode->info->pixel_format == pixel_red_green_blue_reserved_8bit_per_color) {
            fb->pixel_format = FB_PIXEL_FORMAT_ARGB;
        } else {
            fb->pixel_format = FB_PIXEL_FORMAT_UNKNOWN;
        }
    }

    kernel_main();
}

void __attribute__((ms_abi)) efi_entry(efi_handle_t image_handle, efi_system_table_t* system_table)
{
    efi_image_handle = image_handle;
    efi_init(system_table);

    vga_init();
    efi_print_string("Kenux UEFI Boot Loader\n");

    efi_status_t status = efi_setup_framebuffer();
    if (status == EFI_SUCCESS && efi_gop_mode) {
        efi_print_string("GOP framebuffer initialized\n");
        efi_print_string("Resolution: ");
        efi_print_dec(efi_gop_mode->info->horizontal_resolution);
        efi_print_string("x");
        efi_print_dec(efi_gop_mode->info->vertical_resolution);
        efi_print_string("\n");
    } else {
        efi_print_string("GOP framebuffer not available, using VGA\n");
    }

    efi_print_string("Getting memory map...\n");
    status = efi_get_memory_map_and_exit(image_handle, NULL, NULL, NULL, NULL, NULL);
    if (status != EFI_SUCCESS) {
        efi_print_string("Failed to get memory map and exit boot services: ");
        efi_print_hex(status);
        efi_print_string("\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }

    efi_print_string("Boot services exited successfully\n");
    efi_parse_memory_map();

    efi_transition_to_kernel();

    while (1) {
        __asm__ volatile ("hlt");
    }
}
