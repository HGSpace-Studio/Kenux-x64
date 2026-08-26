#include "include/efi.h"
#include "include/fbc.h"
#include "kenux_boot_art.h"

#define NULL 0
#define EFI_FILE_MODE_READ 0x00000001
#define COM1_PORT 0x3F8
#define KERNEL_LOAD_ADDR 0x2000000ULL

static inline void outb(UINT16 port, UINT8 val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_putc(char c) {
    UINT32 guard = 100000;
    while (((inb(COM1_PORT + 5) & 0x20) == 0) && guard--) {
    }
    if (guard == 0) return;
    outb(COM1_PORT, (UINT8)c);
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

static void serial_puthex64(UINT64 val) {
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        int nib = (int)((val >> i) & 0xF);
        serial_putc(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
}

static void xmemset(void *dest, int val, UINTN count) {
    char *d = (char *)dest;
    while (count--) *d++ = val;
}

static void xmemcpy(void *dest, const void *src, UINTN count) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (count--) *d++ = *s++;
}

static void *kmalloc(UINTN size, struct EFI_BOOT_SERVICES *BS) {
    void *buf = NULL;
    BS->AllocatePool(EfiLoaderData, size, &buf);
    return buf;
}

static void kfree(void *buf, struct EFI_BOOT_SERVICES *BS) {
    if (buf) BS->FreePool(buf);
}

static void print(struct EFI_SYSTEM_TABLE *ST, CHAR16 *str) {
    ST->ConOut->OutputString(ST->ConOut, str);
}

static void print_ascii(struct EFI_SYSTEM_TABLE *ST, const char *str) {
    CHAR16 buf[96];
    UINTN i = 0;
    while (str && str[i] && i < 95) {
        buf[i] = (CHAR16)str[i];
        i++;
    }
    buf[i] = 0;
    print(ST, buf);
}

static void boot_text_attr(struct EFI_SYSTEM_TABLE *ST, UINTN attr) {
    if (ST && ST->ConOut && ST->ConOut->SetAttribute) {
        ST->ConOut->SetAttribute(ST->ConOut, attr);
    }
}

static void boot_text_move(struct EFI_SYSTEM_TABLE *ST, UINTN col, UINTN row) {
    if (ST && ST->ConOut && ST->ConOut->SetCursorPosition) {
        ST->ConOut->SetCursorPosition(ST->ConOut, col, row);
    }
}

static void boot_text_pad(struct EFI_SYSTEM_TABLE *ST, const char *text, UINTN width) {
    UINTN len = 0;
    if (text) {
        print_ascii(ST, text);
        while (text[len] && len < width) len++;
    }
    while (len++ < width) {
        print_ascii(ST, " ");
    }
}

static void draw_xj380_style_boot_text(struct EFI_SYSTEM_TABLE *ST, UINT32 progress, const char *stage) {
    if (!ST || !ST->ConOut) return;
    if (ST->ConOut->EnableCursor) ST->ConOut->EnableCursor(ST->ConOut, 0);

    static const char *log_lines[8] = {
        "Firmware graphics ready",
        "Mounting EFI system partition",
        "Opening root filesystem",
        "Locating Kenux kernel image",
        "Reading kernel payload",
        "Preparing framebuffer handoff",
        "Collecting memory map",
        "Starting Kenux kernel"
    };
    UINT32 shown = progress / 13;
    if (shown > 8) shown = 8;

    boot_text_attr(ST, 0x0F);
    boot_text_move(ST, 2, 1);
    boot_text_pad(ST, "Kenux Boot Manager", 74);
    boot_text_move(ST, 2, 3);
    boot_text_attr(ST, 0x0B);
    boot_text_pad(ST, "Starting Kenux", 74);
    boot_text_move(ST, 2, 4);
    boot_text_attr(ST, 0x08);
    boot_text_pad(ST, "Firmware is preparing the operating system.", 74);

    boot_text_move(ST, 2, 7);
    boot_text_attr(ST, 0x07);
    print_ascii(ST, "Current stage: ");
    boot_text_attr(ST, 0x0F);
    boot_text_pad(ST, stage ? stage : "working", 56);

    boot_text_move(ST, 2, 9);
    boot_text_attr(ST, 0x07);
    print_ascii(ST, "[");
    boot_text_attr(ST, 0x0B);
    for (UINT32 i = 0; i < 42; i++) {
        print_ascii(ST, i < (progress * 42u) / 100u ? "=" : " ");
    }
    boot_text_attr(ST, 0x07);
    print_ascii(ST, "] ");
    CHAR16 pct[5];
    pct[0] = (CHAR16)('0' + (progress / 100) % 10);
    pct[1] = (CHAR16)('0' + (progress / 10) % 10);
    pct[2] = (CHAR16)('0' + progress % 10);
    pct[3] = L'%';
    pct[4] = 0;
    print(ST, pct);

    boot_text_move(ST, 2, 12);
    boot_text_attr(ST, 0x08);
    boot_text_pad(ST, "Boot log", 74);
    for (UINT32 i = 0; i < shown; i++) {
        boot_text_move(ST, 4, 14 + i);
        boot_text_attr(ST, 0x07);
        print_ascii(ST, "- ");
        boot_text_pad(ST, log_lines[i], 68);
    }
}

static void print_hex64(struct EFI_SYSTEM_TABLE *ST, UINT64 val) {
    CHAR16 buf[17];
    for (int i = 15; i >= 0; i--) {
        UINT8 nib = (UINT8)((val >> (4 * i)) & 0xF);
        buf[15 - i] = nib < 10 ? '0' + nib : 'A' + nib - 10;
    }
    buf[16] = 0;
    print(ST, buf);
}

static UINT64 mode_score(UINT32 w, UINT32 h) {
    UINT64 pixels = (UINT64)w * h;
    UINT64 ratio = ((UINT64)w * 1000) / h;
    UINT64 diff = ratio > 1778 ? ratio - 1778 : 1778 - ratio;
    if (w < 800 || h < 480) return 0;
    if (pixels > 2304000) pixels = 2304000;
    if (diff > 500) diff = 500;
    UINT64 penalty = diff * 3000;
    return pixels > penalty ? pixels - penalty : 1;
}

static UINT32 splash_color(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP, UINT8 r, UINT8 g, UINT8 b) {
    if (GOP && GOP->Mode && GOP->Mode->Info &&
        GOP->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        return ((UINT32)b << 16) | ((UINT32)g << 8) | r;
    }
    return ((UINT32)r << 16) | ((UINT32)g << 8) | b;
}

static void splash_fill_rect(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                             UINT32 x, UINT32 y, UINT32 w, UINT32 h,
                             UINT32 color) {
    if (!GOP || !GOP->Mode || !GOP->Mode->Info) return;
    UINT32 screen_w = GOP->Mode->Info->HorizontalResolution;
    UINT32 screen_h = GOP->Mode->Info->VerticalResolution;
    UINT32 stride = GOP->Mode->Info->PixelsPerScanLine;
    UINT32 *fb = (UINT32 *)(UINTN)GOP->Mode->FrameBufferBase;
    if (!fb || x >= screen_w || y >= screen_h) return;
    if (x + w > screen_w) w = screen_w - x;
    if (y + h > screen_h) h = screen_h - y;
    for (UINT32 row = 0; row < h; row++) {
        UINT32 *p = fb + (UINTN)(y + row) * stride + x;
        for (UINT32 col = 0; col < w; col++) {
            p[col] = color;
        }
    }
}

static void rgb565_unpack(UINT16 c, UINT8 *r, UINT8 *g, UINT8 *b) {
    UINT8 r5 = (UINT8)((c >> 11) & 0x1F);
    UINT8 g6 = (UINT8)((c >> 5) & 0x3F);
    UINT8 b5 = (UINT8)(c & 0x1F);
    *r = (UINT8)((r5 << 3) | (r5 >> 2));
    *g = (UINT8)((g6 << 2) | (g6 >> 4));
    *b = (UINT8)((b5 << 3) | (b5 >> 2));
}

static void raw_to_rgb(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP, UINT32 raw,
                       UINT8 *r, UINT8 *g, UINT8 *b) {
    if (GOP && GOP->Mode && GOP->Mode->Info &&
        GOP->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        *r = (UINT8)(raw & 0xFF);
        *g = (UINT8)((raw >> 8) & 0xFF);
        *b = (UINT8)((raw >> 16) & 0xFF);
        return;
    }
    *r = (UINT8)((raw >> 16) & 0xFF);
    *g = (UINT8)((raw >> 8) & 0xFF);
    *b = (UINT8)(raw & 0xFF);
}

static void splash_put_pixel(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                             UINT32 x, UINT32 y, UINT32 color) {
    if (!GOP || !GOP->Mode || !GOP->Mode->Info) return;
    UINT32 screen_w = GOP->Mode->Info->HorizontalResolution;
    UINT32 screen_h = GOP->Mode->Info->VerticalResolution;
    if (x >= screen_w || y >= screen_h) return;
    UINT32 stride = GOP->Mode->Info->PixelsPerScanLine;
    UINT32 *fb = (UINT32 *)(UINTN)GOP->Mode->FrameBufferBase;
    if (!fb) return;
    fb[(UINTN)y * stride + x] = color;
}

static UINT32 splash_get_pixel(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                               UINT32 x, UINT32 y) {
    if (!GOP || !GOP->Mode || !GOP->Mode->Info) return 0;
    UINT32 screen_w = GOP->Mode->Info->HorizontalResolution;
    UINT32 screen_h = GOP->Mode->Info->VerticalResolution;
    if (x >= screen_w || y >= screen_h) return 0;
    UINT32 stride = GOP->Mode->Info->PixelsPerScanLine;
    UINT32 *fb = (UINT32 *)(UINTN)GOP->Mode->FrameBufferBase;
    if (!fb) return 0;
    return fb[(UINTN)y * stride + x];
}

static UINT32 alpha_blend_color(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                                UINT32 dst_raw, UINT8 sr, UINT8 sg, UINT8 sb,
                                UINT8 a) {
    UINT8 dr, dg, db;
    raw_to_rgb(GOP, dst_raw, &dr, &dg, &db);
    UINT8 r = (UINT8)(((UINT32)sr * a + (UINT32)dr * (255 - a)) / 255);
    UINT8 g = (UINT8)(((UINT32)sg * a + (UINT32)dg * (255 - a)) / 255);
    UINT8 b = (UINT8)(((UINT32)sb * a + (UINT32)db * (255 - a)) / 255);
    return splash_color(GOP, r, g, b);
}

static void splash_wallpaper(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP) {
    if (!GOP || !GOP->Mode || !GOP->Mode->Info) return;

    UINT32 w = GOP->Mode->Info->HorizontalResolution;
    UINT32 h = GOP->Mode->Info->VerticalResolution;
    UINT32 stride = GOP->Mode->Info->PixelsPerScanLine;
    UINT32 *fb = (UINT32 *)(UINTN)GOP->Mode->FrameBufferBase;
    if (!fb || w == 0 || h == 0) return;

    UINT32 src_w = KENUX_BOOT_WALLPAPER_W;
    UINT32 src_h = KENUX_BOOT_WALLPAPER_H;
    UINT32 scaled_w;
    UINT32 scaled_h;
    UINT32 crop_x = 0;
    UINT32 crop_y = 0;
    int scale_by_width = ((UINT64)w * src_h) >= ((UINT64)h * src_w);

    if (scale_by_width) {
        scaled_w = w;
        scaled_h = (UINT32)(((UINT64)src_h * w + src_w - 1) / src_w);
        if (scaled_h > h) crop_y = (scaled_h - h) / 2;
    } else {
        scaled_h = h;
        scaled_w = (UINT32)(((UINT64)src_w * h + src_h - 1) / src_h);
        if (scaled_w > w) crop_x = (scaled_w - w) / 2;
    }

    for (UINT32 y = 0; y < h; y++) {
        UINT32 src_y = scale_by_width
            ? (UINT32)(((UINT64)(y + crop_y) * src_w) / scaled_w)
            : (UINT32)(((UINT64)y * src_h) / scaled_h);
        if (src_y >= src_h) src_y = src_h - 1;
        UINT32 *row = fb + (UINTN)y * stride;
        for (UINT32 x = 0; x < w; x++) {
            UINT32 src_x = scale_by_width
                ? (UINT32)(((UINT64)x * src_w) / scaled_w)
                : (UINT32)(((UINT64)(x + crop_x) * src_h) / scaled_h);
            if (src_x >= src_w) src_x = src_w - 1;
            UINT8 r, g, b;
            rgb565_unpack(kenux_boot_wallpaper[src_y * src_w + src_x], &r, &g, &b);
            row[x] = splash_color(GOP, r, g, b);
        }
    }
}

static void splash_draw_logo(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                             UINT32 dx, UINT32 dy, UINT32 dw, UINT32 dh) {
    if (!GOP || dw == 0 || dh == 0) return;
    for (UINT32 y = 0; y < dh; y++) {
        UINT32 sy = (UINT32)(((UINT64)y * KENUX_BOOT_LOGO_H) / dh);
        for (UINT32 x = 0; x < dw; x++) {
            UINT32 sx = (UINT32)(((UINT64)x * KENUX_BOOT_LOGO_W) / dw);
            UINT16 p = kenux_boot_logo[sy * KENUX_BOOT_LOGO_W + sx];
            UINT8 a4 = (UINT8)((p >> 12) & 0x0F);
            if (a4 == 0) continue;
            UINT8 a = (UINT8)((a4 << 4) | a4);
            UINT8 r = (UINT8)(((p >> 8) & 0x0F) * 17);
            UINT8 g = (UINT8)(((p >> 4) & 0x0F) * 17);
            UINT8 b = (UINT8)((p & 0x0F) * 17);
            UINT32 px = dx + x;
            UINT32 py = dy + y;
            UINT32 dst = splash_get_pixel(GOP, px, py);
            splash_put_pixel(GOP, px, py, alpha_blend_color(GOP, dst, r, g, b, a));
        }
    }
}

static void draw_kenux_splash(struct EFI_SYSTEM_TABLE *ST,
                              struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                              UINT32 progress,
                              const char *stage) {
    if (!GOP || !GOP->Mode || !GOP->Mode->Info) return;
    (void)ST;
    (void)stage;

    UINT32 w = GOP->Mode->Info->HorizontalResolution;
    UINT32 h = GOP->Mode->Info->VerticalResolution;
    if (progress > 100) progress = 100;

    static UINTN last_fb = 0;
    static UINT32 last_w = 0;
    static UINT32 last_h = 0;
    UINTN fb_base = GOP->Mode->FrameBufferBase;
    if (last_fb != fb_base || last_w != w || last_h != h) {
        splash_wallpaper(GOP);
        last_fb = fb_base;
        last_w = w;
        last_h = h;
    }

    UINT32 accent = splash_color(GOP, 0x7E, 0xA3, 0xC7);
    UINT32 track = splash_color(GOP, 0x14, 0x16, 0x1A);
    UINT32 line = splash_color(GOP, 0xD8, 0xE4, 0xF2);

    UINT32 cx = w / 2;
    UINT32 bar_w = w < 900 ? 300 : 460;
    UINT32 bar_h = w < 900 ? 8 : 10;
    UINT32 bar_x = cx - bar_w / 2;
    UINT32 bar_y = h > 160 ? (h * 82u) / 100u : h - 24;
    UINT32 fill_w = (bar_w - 6) * progress / 100;

    splash_wallpaper(GOP);
    splash_fill_rect(GOP, bar_x, bar_y, bar_w, bar_h, track);
    splash_fill_rect(GOP, bar_x, bar_y, bar_w, 1, line);
    splash_fill_rect(GOP, bar_x + 3, bar_y + 3, fill_w, bar_h > 6 ? bar_h - 6 : 1, accent);
}

static void draw_qemu_uefi_countdown(struct EFI_SYSTEM_TABLE *ST,
                                     struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                                     UINT32 seconds_left) {
    if (GOP) {
        draw_kenux_splash(ST, GOP, 96, "QEMU UEFI handoff countdown");
    }
    if (!ST || !ST->ConOut) return;

    boot_text_move(ST, 2, 24);
    boot_text_attr(ST, 0x0F);
    boot_text_pad(ST, "QEMU UEFI Boot Manager", 74);
    boot_text_move(ST, 2, 25);
    boot_text_attr(ST, 0x0B);
    print_ascii(ST, "Booting Kenux in ");
    CHAR16 sec[2];
    sec[0] = (CHAR16)('0' + (seconds_left % 10));
    sec[1] = 0;
    print(ST, sec);
    print_ascii(ST, " second(s). Press reset to interrupt.       ");
}

static void qemu_uefi_boot_countdown(struct EFI_SYSTEM_TABLE *ST,
                                     struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP,
                                     struct EFI_BOOT_SERVICES *BS) {
    (void)ST;
    (void)GOP;
    (void)BS;
}

typedef struct {
    UINT16 e_magic;
    UINT16 e_cblp;
    UINT16 e_cp;
    UINT16 e_crlc;
    UINT16 e_cparhdr;
    UINT16 e_minalloc;
    UINT16 e_maxalloc;
    UINT16 e_ss;
    UINT16 e_sp;
    UINT16 e_csum;
    UINT16 e_ip;
    UINT16 e_cs;
    UINT16 e_lfarlc;
    UINT16 e_ovno;
    UINT16 e_res[4];
    UINT16 e_oemid;
    UINT16 e_oeminfo;
    UINT16 e_res2[10];
    UINT32 e_lfanew;
} IMAGE_DOS_HEADER;

typedef struct {
    UINT16 Machine;
    UINT16 NumberOfSections;
    UINT32 TimeDateStamp;
    UINT32 PointerToSymbolTable;
    UINT32 NumberOfSymbols;
    UINT16 SizeOfOptionalHeader;
    UINT16 Characteristics;
} IMAGE_FILE_HEADER;

typedef struct {
    UINT32 VirtualAddress;
    UINT32 Size;
} IMAGE_DATA_DIRECTORY;

typedef struct {
    UINT16 Magic;
    UINT8 MajorLinkerVersion;
    UINT8 MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT64 SizeOfStackReserve;
    UINT64 SizeOfStackCommit;
    UINT64 SizeOfHeapReserve;
    UINT64 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

typedef struct {
    UINT8 Name[8];
    UINT32 VirtualSize;
    UINT32 VirtualAddress;
    UINT32 SizeOfRawData;
    UINT32 PointerToRawData;
    UINT32 PointerToRelocations;
    UINT32 PointerToLinenumbers;
    UINT16 NumberOfRelocations;
    UINT16 NumberOfLinenumbers;
    UINT32 Characteristics;
} IMAGE_SECTION_HEADER;

#define IMAGE_FILE_MACHINE_AMD64 0x8664

typedef struct {
    UINT8  e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} ELF64_EHDR;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} ELF64_PHDR;

#define ELF_MAGIC0 0x7f
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELFCLASS64 2
#define ET_EXEC 2
#define EM_X86_64 62
#define PT_LOAD 1

static UINT64 align_down(UINT64 value, UINT64 align) {
    return value & ~(align - 1);
}

static UINT64 align_up(UINT64 value, UINT64 align) {
    return (value + align - 1) & ~(align - 1);
}

static UINT64 load_elf64_exec(void *buf, UINTN len, struct EFI_SYSTEM_TABLE *ST) {
    ELF64_EHDR *eh = (ELF64_EHDR *)buf;
    UINT64 low = 0xffffffffffffffffULL;
    UINT64 high = 0;

    if (len < sizeof(ELF64_EHDR) ||
        eh->e_ident[0] != ELF_MAGIC0 || eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 || eh->e_ident[3] != ELF_MAGIC3 ||
        eh->e_ident[4] != ELFCLASS64 || eh->e_type != ET_EXEC ||
        eh->e_machine != EM_X86_64 ||
        eh->e_phoff + ((UINT64)eh->e_phnum * eh->e_phentsize) > len) {
        print(ST, L"ELF64 kernel invalid\n");
        return 0;
    }

    for (UINT16 i = 0; i < eh->e_phnum; i++) {
        ELF64_PHDR *ph = (ELF64_PHDR *)((UINT8 *)buf + eh->e_phoff + ((UINT64)i * eh->e_phentsize));
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_offset + ph->p_filesz > len || ph->p_filesz > ph->p_memsz) {
            print(ST, L"ELF64 segment invalid\n");
            return 0;
        }
        UINT64 dst = ph->p_paddr ? ph->p_paddr : ph->p_vaddr;
        UINT64 seg_start = align_down(dst, 4096);
        UINT64 seg_end = align_up(dst + ph->p_memsz, 4096);
        if (!dst || seg_end <= seg_start) {
            print(ST, L"ELF64 segment address invalid\n");
            return 0;
        }
        if (seg_start < low) low = seg_start;
        if (seg_end > high) high = seg_end;
    }

    if (low == 0xffffffffffffffffULL || high <= low) {
        print(ST, L"ELF64 has no loadable segments\n");
        return 0;
    }

    struct EFI_BOOT_SERVICES *BS = ST->BootServices;
    EFI_PHYSICAL_ADDRESS alloc_addr = low;
    UINTN pages = (UINTN)((high - low) / 4096);
    EFI_STATUS alloc_status = BS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &alloc_addr);
    if (EFI_ERROR(alloc_status)) {
        print(ST, L"ELF64 AllocatePages failed at 0x");
        print_hex64(ST, low);
        print(ST, L"\n");
        return 0;
    }
    xmemset((void *)(UINTN)low, 0, (UINTN)(high - low));

    for (UINT16 i = 0; i < eh->e_phnum; i++) {
        ELF64_PHDR *ph = (ELF64_PHDR *)((UINT8 *)buf + eh->e_phoff + ((UINT64)i * eh->e_phentsize));
        if (ph->p_type != PT_LOAD) continue;
        UINT64 dst = ph->p_paddr ? ph->p_paddr : ph->p_vaddr;
        xmemcpy((void *)(UINTN)dst, (UINT8 *)buf + ph->p_offset, (UINTN)ph->p_filesz);
    }

    print(ST, L"ELF64 kernel loaded entry=0x");
    print_hex64(ST, eh->e_entry);
    print(ST, L"\n");
    return eh->e_entry;
}

static UINT64 load_pe64(void *buf, struct EFI_SYSTEM_TABLE *ST) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)buf;
    if (dos->e_magic != 0x5a4d) {
        print(ST, L"Not MZ\n");
        return 0;
    }

    UINT32 pe_off = dos->e_lfanew;
    UINT32 *pe_sig = (UINT32 *)((UINT8 *)buf + pe_off);
    if (*pe_sig != 0x00004550) {
        print(ST, L"PE sig bad\n");
        return 0;
    }

    IMAGE_FILE_HEADER *coff = (IMAGE_FILE_HEADER *)((UINT8 *)buf + pe_off + 4);
    if (coff->Machine != IMAGE_FILE_MACHINE_AMD64) {
        print(ST, L"Not AMD64\n");
        return 0;
    }

    IMAGE_OPTIONAL_HEADER64 *opt = (IMAGE_OPTIONAL_HEADER64 *)((UINT8 *)coff + sizeof(IMAGE_FILE_HEADER));
    if (opt->Magic != 0x20b) {
        print(ST, L"Not PE32+\n");
        return 0;
    }

    UINT64 image_base = opt->ImageBase;
    UINT64 entry = image_base + opt->AddressOfEntryPoint;

    print(ST, L"PE32+ OK, base=0x");
    print_hex64(ST, image_base);
    print(ST, L" entry=0x");
    print_hex64(ST, entry);
    print(ST, L"\n");

    struct EFI_BOOT_SERVICES *BS = ST->BootServices;
    UINTN num_pages = (opt->SizeOfImage + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS alloc_addr = image_base;
    EFI_STATUS alloc_status = BS->AllocatePages(AllocateAddress, EfiLoaderData, num_pages, &alloc_addr);
    if (EFI_ERROR(alloc_status)) {
        print(ST, L"Alloc failed\n");
        return 0;
    }

    xmemset((void *)(UINTN)image_base, 0, (UINTN)opt->SizeOfImage);
    xmemcpy((void *)(UINTN)image_base, buf, (UINTN)opt->SizeOfHeaders);

    IMAGE_SECTION_HEADER *sect = (IMAGE_SECTION_HEADER *)((UINT8 *)opt + coff->SizeOfOptionalHeader);
    for (UINTN i = 0; i < coff->NumberOfSections; i++) {
        if (sect[i].VirtualAddress >= opt->SizeOfImage) {
            continue;
        }
        UINT8 *src = (UINT8 *)buf + sect[i].PointerToRawData;
        UINT8 *dst = (UINT8 *)(UINTN)(image_base + sect[i].VirtualAddress);
        UINTN sz = (UINTN)sect[i].SizeOfRawData;
        if (sz > 0) {
            xmemcpy(dst, src, sz);
        }
    }

    print(ST, L"PE loaded\n");
    return entry;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable) {
    serial_puts("[BOOT] efi_main entry\n");

    struct EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP = NULL;
    struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp = NULL;
    struct EFI_FILE_PROTOCOL *root = NULL;
    struct EFI_FILE_PROTOCOL *file = NULL;
    EFI_STATUS status;
    int kernel_is_elf = 0;

    print(SystemTable, L"\nKenux UEFI Bootloader\n");
    print(SystemTable, L"========================\n");
    serial_puts("[BOOT] Kenux UEFI Bootloader\n");

    EFI_GUID gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
    print(SystemTable, L"Locating GOP...\n");
    serial_puts("[BOOT] Locating GOP...\n");
    status = BS->LocateProtocol(&gop_guid, NULL, (void **)&GOP);
    if (EFI_ERROR(status)) GOP = NULL;
    serial_puts(GOP ? "[BOOT] GOP found\n" : "[BOOT] GOP not found\n");
    if (GOP) {
        UINT32 best_mode = GOP->Mode->Mode;
        UINT64 best_score = mode_score(GOP->Mode->Info->HorizontalResolution, GOP->Mode->Info->VerticalResolution);
        for (UINT32 i = 0; i < GOP->Mode->MaxMode; i++) {
            UINTN sz = 0;
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
            if (GOP->QueryMode(GOP, i, &sz, &info) == EFI_SUCCESS) {
                UINT64 score = mode_score(info->HorizontalResolution, info->VerticalResolution);
                if (score > best_score) {
                    best_score = score;
                    best_mode = i;
                }
            }
        }
        GOP->SetMode(GOP, best_mode);
        draw_kenux_splash(SystemTable, GOP, 12, "Graphics ready");
    }

    EFI_GUID sfsp_guid = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    print(SystemTable, L"Locating SFSP...\n");
    status = BS->LocateProtocol(&sfsp_guid, NULL, (void **)&sfsp);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"SFSP not found\n");
        while (1);
    }
    if (GOP) draw_kenux_splash(SystemTable, GOP, 24, "Locating EFI system partition");

    print(SystemTable, L"Opening root...\n");
    status = sfsp->OpenVolume(sfsp, &root);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"OpenVolume failed\n");
        while (1);
    }
    if (GOP) draw_kenux_splash(SystemTable, GOP, 34, "Opening root filesystem");

    print(SystemTable, L"Loading kernel ELF...\n");
    status = root->Open(root, &file, L"kernel.elf", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(status)) {
        kernel_is_elf = 1;
    } else {
        print(SystemTable, L"kernel.elf missing, fallback to KENUXK.BIN\n");
        status = root->Open(root, &file, L"KENUXK.BIN", EFI_FILE_MODE_READ, 0);
    }
    if (EFI_ERROR(status)) {
        print(SystemTable, L"Open kernel file failed\n");
        while (1);
    }
    if (GOP) draw_kenux_splash(SystemTable, GOP, 46, "Locating Kenux kernel image");

    EFI_GUID file_info_guid = {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    UINTN info_size = sizeof(EFI_FILE_INFO);
    EFI_FILE_INFO *info = (EFI_FILE_INFO *)kmalloc(info_size, BS);
    file->GetInfo(file, &file_info_guid, &info_size, (void *)info);
    if (info_size > sizeof(EFI_FILE_INFO)) {
        kfree(info, BS);
        info = (EFI_FILE_INFO *)kmalloc(info_size, BS);
        file->GetInfo(file, &file_info_guid, &info_size, (void *)info);
    }
    UINTN kernel_size = (UINTN)info->FileSize;
    kfree(info, BS);

    print(SystemTable, L"Kernel size: ");
    {
        CHAR16 numbuf[20];
        UINTN n = kernel_size;
        int i = 19;
        numbuf[i--] = 0;
        if (n == 0) numbuf[i--] = '0';
        while (n > 0 && i >= 0) {
            numbuf[i--] = '0' + (n % 10);
            n /= 10;
        }
        print(SystemTable, &numbuf[i + 1]);
        print(SystemTable, L" bytes\n");
    }

    void *kernel_buf = kmalloc(kernel_size, BS);
    if (!kernel_buf) {
        print(SystemTable, L"Malloc failed\n");
        while (1);
    }
    UINTN read_size = kernel_size;
    file->Read(file, &read_size, kernel_buf);
    file->Close(file);
    if (GOP) draw_kenux_splash(SystemTable, GOP, 60, "Reading kernel payload");

    UINT64 entry = 0;
    if (kernel_is_elf) {
        entry = load_elf64_exec(kernel_buf, kernel_size, SystemTable);
        kfree(kernel_buf, BS);
        if (!entry) {
            print(SystemTable, L"Load kernel.elf failed\n");
            while (1);
        }
    } else {
        UINTN kernel_pages = (kernel_size + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS load_addr = KERNEL_LOAD_ADDR;
        status = BS->AllocatePages(AllocateAddress, EfiLoaderData, kernel_pages, &load_addr);
        if (EFI_ERROR(status)) {
            print(SystemTable, L"AllocatePages at kernel load address failed\n");
            while (1);
        }
        xmemcpy((void *)(UINTN)KERNEL_LOAD_ADDR, kernel_buf, kernel_size);
        kfree(kernel_buf, BS);
        print(SystemTable, L"Kernel loaded at 0x2000000\n");
        entry = KERNEL_LOAD_ADDR;
    }
    if (GOP) draw_kenux_splash(SystemTable, GOP, 76, "Preparing framebuffer handoff");

    if (GOP) {
        draw_kenux_splash(SystemTable, GOP, 84, "Collecting memory map");
    }

    struct FrameBufferConfig fbc;
    xmemset(&fbc, 0, sizeof(fbc));
    if (GOP) {
        fbc.frame_buffer = (UINT8 *)GOP->Mode->FrameBufferBase;
        fbc.pixels_per_scan_line = GOP->Mode->Info->PixelsPerScanLine;
        fbc.horizontal_resolution = GOP->Mode->Info->HorizontalResolution;
        fbc.vertical_resolution = GOP->Mode->Info->VerticalResolution;
        if (GOP->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            fbc.pixel_format = kPixelBGRR;
        } else {
            fbc.pixel_format = kPixelRGBR;
        }
    }

    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map_buf = NULL;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;
    BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 4096 * 4;
    UINTN map_buf_size = map_size;
    map_buf = (EFI_MEMORY_DESCRIPTOR *)kmalloc(map_buf_size, BS);

    print(SystemTable, L"Exiting boot services...\n");
    serial_puts("[BOOT] Exiting boot services...\n");
    if (GOP) draw_kenux_splash(SystemTable, GOP, 94, "Starting Kenux kernel");
    qemu_uefi_boot_countdown(SystemTable, GOP, BS);
    do {
        map_size = map_buf_size;
        BS->GetMemoryMap(&map_size, map_buf, &map_key, &desc_size, &desc_ver);
        status = BS->ExitBootServices(ImageHandle, map_key);
    } while (EFI_ERROR(status));

    serial_puts("[BOOT] Boot services exited, jumping to kernel...\n");

    struct MemoryMapInfo mmi;
    mmi.buffer_size = map_buf_size;
    mmi.map_size = map_size;
    mmi.descriptor_size = desc_size;
    mmi.descriptor_version = desc_ver;
    mmi.buffer = map_buf;

    typedef void (*KernelEntry)(const struct FrameBufferConfig *, const struct MemoryMapInfo *);
    KernelEntry kernel = (KernelEntry)entry;
    kernel(&fbc, &mmi);

    while (1);
    return EFI_SUCCESS;
}
