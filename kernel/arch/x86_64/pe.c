
#include <arch/pe.h>
#include <string.h>
#include <arch/slab.h>

static inline IMAGE_NT_HEADERS64* pe_get_nt_headers(const uint8_t* buffer)
{
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)buffer;
    return (IMAGE_NT_HEADERS64*)(buffer + (uintptr_t)dos->e_lfanew);
}

int pe_validate(const uint8_t* buffer, uint64_t size)
{
    if (size < (uint64_t)sizeof(IMAGE_DOS_HEADER)) {
        return -1;
    }

    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)buffer;
    if (dos->e_magic != (uint16_t)IMAGE_DOS_SIGNATURE) {
        return -1;
    }

    if ((uint64_t)dos->e_lfanew + (uint64_t)sizeof(IMAGE_NT_HEADERS64) > size) {
        return -1;
    }

    const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(buffer + (uintptr_t)dos->e_lfanew);
    if (nt->Signature != (uint32_t)IMAGE_NT_SIGNATURE) {
        return -1;
    }

    if (nt->OptionalHeader.Magic != (uint16_t)IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return -1;
    }

    if (nt->FileHeader.Machine != (uint16_t)IMAGE_FILE_MACHINE_AMD64) {
        return -1;
    }

    return 0;
}

uint64_t pe_get_entry_point(const uint8_t* buffer)
{
    const IMAGE_NT_HEADERS64* nt = pe_get_nt_headers(buffer);
    return (uint64_t)((uint64_t)nt->OptionalHeader.AddressOfEntryPoint + nt->OptionalHeader.ImageBase);
}

uint64_t pe_get_image_base(const uint8_t* buffer)
{
    const IMAGE_NT_HEADERS64* nt = pe_get_nt_headers(buffer);
    return nt->OptionalHeader.ImageBase;
}

uint32_t pe_get_image_size(const uint8_t* buffer)
{
    const IMAGE_NT_HEADERS64* nt = pe_get_nt_headers(buffer);
    return nt->OptionalHeader.SizeOfImage;
}

uint64_t pe_get_rva(const uint8_t* buffer, int directory_index, uint32_t* out_size)
{
    const IMAGE_NT_HEADERS64* nt = pe_get_nt_headers(buffer);
    const IMAGE_DATA_DIRECTORY* dir = &nt->OptionalHeader.DataDirectory[directory_index];
    if (out_size != (uint32_t*)0) {
        *out_size = dir->Size;
    }
    return (uint64_t)dir->VirtualAddress;
}

int pe_load_sections(const uint8_t* file_buf, uint8_t* image_base)
{
    const IMAGE_NT_HEADERS64* nt = pe_get_nt_headers(file_buf);
    const uint16_t num_sections = nt->FileHeader.NumberOfSections;
    const IMAGE_SECTION_HEADER* sec = (const IMAGE_SECTION_HEADER*)(
        (const uint8_t*)&nt->OptionalHeader + (uintptr_t)nt->FileHeader.SizeOfOptionalHeader);

    for (uint16_t i = 0; i < num_sections; i++) {
        uint32_t virtual_size = sec[i].Misc.VirtualSize;
        if (virtual_size == 0U) {
            virtual_size = sec[i].SizeOfRawData;
        }

        uint32_t raw_size = sec[i].SizeOfRawData;
        uint32_t clear_size = (virtual_size > raw_size) ? virtual_size : raw_size;

        uint8_t* dest = image_base + (uintptr_t)sec[i].VirtualAddress;
        memset(dest, 0, (size_t)clear_size);

        if (raw_size > 0U) {
            const uint8_t* src = file_buf + (uintptr_t)sec[i].PointerToRawData;
            memcpy(dest, src, (size_t)raw_size);
        }
    }

    return 0;
}

int pe_apply_relocations(const uint8_t* file_buf, uint8_t* image_base, uint64_t actual_base)
{
    const uint64_t preferred = pe_get_image_base(file_buf);
    const int64_t delta = (int64_t)actual_base - (int64_t)preferred;
    if (delta == 0) {
        return 0;
    }

    uint32_t reloc_size = 0U;
    const uint64_t reloc_rva = pe_get_rva(file_buf, IMAGE_DIRECTORY_ENTRY_BASERELOC, &reloc_size);
    if (reloc_rva == 0U) {
        return 0;
    }

    IMAGE_BASE_RELOCATION* block = (IMAGE_BASE_RELOCATION*)(image_base + (uintptr_t)reloc_rva);
    while (block->VirtualAddress != 0U && block->SizeOfBlock != 0U) {
        const uint32_t count = (uint32_t)((block->SizeOfBlock - 8U) / 2U);
        const uint16_t* entries = (const uint16_t*)((const uint8_t*)block + 8U);

        for (uint32_t i = 0U; i < count; i++) {
            const uint16_t entry = entries[i];
            if ((uint16_t)(entry >> 12) == (uint16_t)IMAGE_REL_BASED_DIR64) {
                const uint32_t offset = (uint32_t)(entry & 0xFFFU);
                uint64_t* fixup_addr = (uint64_t*)(image_base + (uintptr_t)block->VirtualAddress + (uintptr_t)offset);
                *fixup_addr = (uint64_t)((int64_t)*fixup_addr + delta);
            }
        }

        block = (IMAGE_BASE_RELOCATION*)((uint8_t*)block + (uintptr_t)block->SizeOfBlock);
    }

    return 0;
}

int pe_resolve_imports(const uint8_t* file_buf, uint8_t* image_base,
                        void* (*resolve_func)(const char* dll, const char* name, uint16_t ordinal))
{
    uint32_t import_size = 0U;
    const uint64_t import_rva = pe_get_rva(file_buf, IMAGE_DIRECTORY_ENTRY_IMPORT, &import_size);
    if (import_rva == 0U) {
        return 0;
    }

    IMAGE_IMPORT_DESCRIPTOR* import_desc = (IMAGE_IMPORT_DESCRIPTOR*)(image_base + (uintptr_t)import_rva);
    while (import_desc->Name != 0U) {
        const char* dll_name = (const char*)(image_base + (uintptr_t)import_desc->Name);

        uint64_t thunk_rva;
        if (import_desc->u.OriginalFirstThunk != 0U) {
            thunk_rva = (uint64_t)import_desc->u.OriginalFirstThunk;
        } else {
            thunk_rva = (uint64_t)import_desc->FirstThunk;
        }

        IMAGE_THUNK_DATA64* thunk = (IMAGE_THUNK_DATA64*)(image_base + (uintptr_t)thunk_rva);
        uint64_t* iat = (uint64_t*)(image_base + (uintptr_t)import_desc->FirstThunk);

        while (thunk->u1.AddressOfData != 0ULL) {
            if ((thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) != 0ULL) {
                const uint16_t ord = (uint16_t)(thunk->u1.Ordinal & 0xFFFFULL);
                *iat = (uint64_t)(uintptr_t)resolve_func(dll_name, (const char*)0, ord);
            } else {
                const char* import_name = (const char*)(image_base + (uintptr_t)thunk->u1.AddressOfData + 2U);
                *iat = (uint64_t)(uintptr_t)resolve_func(dll_name, import_name, 0U);
            }
            thunk++;
            iat++;
        }

        import_desc++;
    }

    return 0;
}

int pe_run_tls_callbacks(const uint8_t* file_buf, uint8_t* image_base,
                          void* (*get_proc_addr)(const char*))
{
    (void)get_proc_addr;

    uint32_t tls_size = 0U;
    const uint64_t tls_rva = pe_get_rva(file_buf, IMAGE_DIRECTORY_ENTRY_TLS, &tls_size);
    if (tls_rva == 0U || tls_size == 0U) {
        return 0;
    }

    IMAGE_TLS_DIRECTORY64* tls = (IMAGE_TLS_DIRECTORY64*)(image_base + (uintptr_t)tls_rva);
    if (tls->AddressOfCallBacks == 0ULL) {
        return 0;
    }

    const uint64_t preferred_base = pe_get_image_base(file_buf);
    const uint64_t callbacks_rva = tls->AddressOfCallBacks - preferred_base;
    void (**callback_arr)(void) = (void (**)(void))(image_base + (uintptr_t)callbacks_rva);

    for (int i = 0; callback_arr[i] != (void (*)(void))0; i++) {
        callback_arr[i]();
    }

    return 0;
}
