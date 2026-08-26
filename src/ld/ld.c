/*
 * Kenux OS - Linker Implementation
 * Main linker functionality
 */

#include "ld.h"

#include <stdint.h>
#include <stddef.h>

#ifndef _WIN32
#include <sys/mman.h>
#endif

// ELF constants
#define ELFMAG "\x7fELF"
#define EI_NIDENT 16
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFIDENT_DATA 5
#define ELFIDENT_VERSION 6

#define EM_386 3
#define EM_X86_64 62

#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3

#define SHN_UNDEF 0
#define SHN_ABS 0xfff1

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STV_DEFAULT 0
#define STV_INTERNAL 1
#define STV_HIDDEN 2

/* Section header types */
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_HASH     5
#define SHT_DYNAMIC  6
#define SHT_NOTE     7
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHT_SHLIB    10
#define SHT_DYNSYM   11

/* ELF32 base types */
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

/* ELF32 symbol table entry (Elf32_Sym) */
typedef struct {
    Elf32_Word    st_name;    /* offset in strtab */
    Elf32_Addr    st_value;   /* value/address */
    Elf32_Word    st_size;    /* size of symbol */
    unsigned char st_info;    /* type + binding */
    unsigned char st_other;   /* visibility */
    Elf32_Half    st_shndx;   /* section index */
} Elf32_Sym;

/* ELF32 section header (Elf32_Shdr) */
typedef struct {
    Elf32_Word    sh_name;      /* name offset in shstrtab */
    Elf32_Word    sh_type;      /* section type */
    Elf32_Word    sh_flags;     /* flags */
    Elf32_Addr    sh_addr;      /* virtual address */
    Elf32_Off     sh_offset;     /* file offset */
    Elf32_Word    sh_size;      /* section size */
    Elf32_Word    sh_link;      /* link to another section */
    Elf32_Word    sh_info;      /* extra info */
    Elf32_Word    sh_addralign; /* alignment */
    Elf32_Word    sh_entsize;   /* entry size if table */
} Elf32_Shdr;

/* ELF32 file header (Elf32_Ehdr) — only the fields used by this linker */
typedef struct {
    unsigned char e_ident[16]; /* magic + class + data + version + ... */
    Elf32_Half    e_type;      /* ET_REL/ET_EXEC/ET_DYN */
    Elf32_Half    e_machine;   /* EM_386/EM_X86_64 */
    Elf32_Word    e_version;   /* EV_CURRENT */
    Elf32_Addr    e_entry;     /* entry point */
    Elf32_Off     e_phoff;     /* program header table offset */
    Elf32_Off     e_shoff;     /* section header table offset */
    Elf32_Word    e_flags;     /* processor flags */
    Elf32_Half    e_ehsize;    /* ELF header size */
    Elf32_Half    e_phentsize; /* program header entry size */
    Elf32_Half    e_phnum;     /* number of program headers */
    Elf32_Half    e_shentsize; /* section header entry size */
    Elf32_Half    e_shnum;     /* number of section headers */
    Elf32_Half    e_shstrndx;  /* section header string table index */
} Elf32_Ehdr;

/* Symbol info accessors */
#define ELF32_ST_BIND(i)  ((i) >> 4)
#define ELF32_ST_TYPE(i)  ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

void linker_init(LinkerState *state) {
    memset(state, 0, sizeof(LinkerState));
    state->verbose = 0;
    state->strip_debug = 0;
    state->create_shared = 0;
    state->pie_enabled = 0;
}

void linker_cleanup(LinkerState *state) {
    // Close object files
    for (int i = 0; i < state->num_objects; i++) {
        if (state->objects[i].fd >= 0) {
            close(state->objects[i].fd);
        }
        if (state->objects[i].sections) {
            for (int j = 0; j < state->objects[i].num_sections; j++) {
                if (state->objects[i].sections[j].data) {
                    free(state->objects[i].sections[j].data);
                }
            }
        }
    }
    
    // Close output file
    if (state->output_fd >= 0) {
        close(state->output_fd);
    }
}

int parse_arguments(LinkerState *state, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            strcpy(state->output_file, argv[++i]);
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            strcpy(state->entry_point, argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            state->verbose = 1;
        } else if (strcmp(argv[i], "--strip-debug") == 0) {
            state->strip_debug = 1;
        } else if (strcmp(argv[i], "--shared") == 0) {
            state->create_shared = 1;
        } else if (strcmp(argv[i], "-pie") == 0) {
            state->pie_enabled = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            // Object file
            if (state->num_objects < MAX_OBJECT_FILES) {
                if (load_object_file(state, argv[i]) != 0) {
                    fprintf(stderr, "kenux-ld: cannot load object file: %s\n", argv[i]);
                    return -1;
                }
            } else {
                fprintf(stderr, "kenux-ld: too many object files\n");
                return -1;
            }
        } else {
            fprintf(stderr, "kenux-ld: unknown option: %s\n", argv[i]);
            return -1;
        }
    }
    
    // Set default output file
    if (strlen(state->output_file) == 0) {
        strcpy(state->output_file, "a.out");
    }
    
    // Set default entry point
    if (strlen(state->entry_point) == 0 && state->num_objects > 0) {
        // Try to find entry point in first object
        for (int i = 0; i < state->objects[0].num_symbols; i++) {
            if (state->objects[0].symbols[i].type == SYMBOL_FUNCTION) {
                strcpy(state->entry_point, state->objects[0].symbols[i].name);
                break;
            }
        }
    }
    
    return 0;
}

int load_object_file(LinkerState *state, const char *filename) {
    ObjectFile *obj = &state->objects[state->num_objects];
    
    // Initialize object file
    memset(obj, 0, sizeof(ObjectFile));
    strcpy(obj->filename, filename);
    
    // Open file
    obj->fd = open(filename, O_RDONLY);
    if (obj->fd < 0) {
        fprintf(stderr, "kenux-ld: cannot open file: %s\n", filename);
        return -1;
    }
    
    // Get file size
    struct stat st;
    if (fstat(obj->fd, &st) != 0) {
        fprintf(stderr, "kenux-ld: cannot get file size: %s\n", filename);
        close(obj->fd);
        return -1;
    }
    obj->file_size = st.st_size;
    
    // Parse ELF header
    if (parse_elf_header(state, obj) != 0) {
        close(obj->fd);
        return -1;
    }
    
    // Parse section headers
    if (parse_section_headers(state, obj) != 0) {
        close(obj->fd);
        return -1;
    }
    
    // Parse symbol table
    if (parse_symbol_table(state, obj) != 0) {
        close(obj->fd);
        return -1;
    }
    
    if (state->verbose) {
        printf("Loaded object file: %s\n", filename);
        printf("  Entry point: 0x%lx\n", obj->entry_point);
        printf("  Sections: %d\n", obj->num_sections);
        printf("  Symbols: %d\n", obj->num_symbols);
    }
    
    state->num_objects++;
    return 0;
}

int parse_elf_header(LinkerState *state, ObjectFile *obj) {
    unsigned char header[EI_NIDENT];
    unsigned int e_shoff, e_shentsize, e_shnum, e_shstrndx;
    
    // Read ELF identification
    if (read(obj->fd, header, EI_NIDENT) != EI_NIDENT) {
        fprintf(stderr, "kenux-ld: cannot read ELF header\n");
        return -1;
    }
    
    // Check ELF magic
    if (memcmp(header, ELFMAG, 4) != 0) {
        fprintf(stderr, "kenux-ld: not an ELF file\n");
        return -1;
    }
    
    // Read ELF header (simplified - just getting essential fields)
    lseek(obj->fd, offsetof(Elf32_Ehdr, e_entry), SEEK_SET);
    read(obj->fd, &obj->entry_point, sizeof(Elf32_Addr));
    
    lseek(obj->fd, offsetof(Elf32_Ehdr, e_shoff), SEEK_SET);
    read(obj->fd, &e_shoff, sizeof(Elf32_Off));
    
    lseek(obj->fd, offsetof(Elf32_Ehdr, e_shentsize), SEEK_SET);
    read(obj->fd, &e_shentsize, sizeof(Elf32_Half));
    
    lseek(obj->fd, offsetof(Elf32_Ehdr, e_shnum), SEEK_SET);
    read(obj->fd, &e_shnum, sizeof(Elf32_Half));
    
    lseek(obj->fd, offsetof(Elf32_Ehdr, e_shstrndx), SEEK_SET);
    read(obj->fd, &e_shstrndx, sizeof(Elf32_Half));
    
    return 0;
}

int parse_section_headers(LinkerState *state, ObjectFile *obj) {
    Elf32_Shdr shdr;
    char shstrtab[256];
    
    // Read section header string table
    lseek(obj->fd, 0, SEEK_SET);
    read(obj->fd, shstrtab, sizeof(Elf32_Ehdr));
    
    lseek(obj->fd, offsetof(Elf32_Ehdr, e_shstrndx), SEEK_SET);
    read(obj->fd, &obj->num_sections, sizeof(Elf32_Half));
    
    // For each section, create section info
    for (int i = 0; i < obj->num_sections; i++) {
        Section *section = &obj->sections[i];
        
        // Read section header
        lseek(obj->fd, (i + 1) * sizeof(Elf32_Shdr), SEEK_SET);
        read(obj->fd, &shdr, sizeof(Elf32_Shdr));
        
        // Get section name
        lseek(obj->fd, shdr.sh_name, SEEK_SET);
        read(obj->fd, section->name, sizeof(section->name) - 1);
        
        // Set section properties
        section->type = SECTION_UNKNOWN;
        if (shdr.sh_flags & SHF_EXECINSTR) {
            section->type = SECTION_CODE;
        } else if (shdr.sh_flags & SHF_WRITE) {
            section->type = SECTION_DATA;
        } else if (shdr.sh_type == SHT_NOBITS) {
            section->type = SECTION_BSS;
        } else if (shdr.sh_flags & SHF_ALLOC) {
            section->type = SECTION_RODATA;
        }
        
        section->size = shdr.sh_size;
        section->virtual_address = shdr.sh_addr;
        section->file_offset = shdr.sh_offset;
        section->alignment = shdr.sh_addralign;
        
        // Allocate memory for section data
        if (section->size > 0 && section->type != SECTION_BSS) {
            section->data = malloc(section->size);
            if (!section->data) {
                fprintf(stderr, "kenux-ld: out of memory\n");
                return -1;
            }
            
            // Read section data
            lseek(obj->fd, section->file_offset, SEEK_SET);
            read(obj->fd, section->data, section->size);
        }
    }
    
    return 0;
}

int parse_symbol_table(LinkerState *state, ObjectFile *obj) {
    Elf32_Shdr shdr;
    Elf32_Sym sym;
    char strtab[1024];
    
    // Find symbol table
    for (int i = 0; i < obj->num_sections; i++) {
        lseek(obj->fd, (i + 1) * sizeof(Elf32_Shdr), SEEK_SET);
        read(obj->fd, &shdr, sizeof(Elf32_Shdr));
        
        if (shdr.sh_type == SHT_SYMTAB) {
            // Read symbol table
            obj->num_symbols = shdr.sh_size / sizeof(Elf32_Sym);
            
            // Read string table
            lseek(obj->fd, shdr.sh_link * sizeof(Elf32_Shdr), SEEK_SET);
            read(obj->fd, &shdr, sizeof(Elf32_Shdr));
            
            lseek(obj->fd, shdr.sh_offset, SEEK_SET);
            read(obj->fd, strtab, shdr.sh_size);
            
            // Read each symbol
            for (int j = 0; j < obj->num_symbols; j++) {
                Symbol *symbol = &obj->symbols[j];
                
                lseek(obj->fd, shdr.sh_offset + j * sizeof(Elf32_Sym), SEEK_SET);
                read(obj->fd, &sym, sizeof(Elf32_Sym));
                
                // Get symbol name
                strcpy(symbol->name, strtab + sym.st_name);
                
                // Set symbol properties
                symbol->type = SYMBOL_UNDEF;
                if (ELF32_ST_TYPE(sym.st_info) == STT_FUNC) {
                    symbol->type = SYMBOL_FUNCTION;
                } else if (ELF32_ST_TYPE(sym.st_info) == STT_OBJECT) {
                    symbol->type = SYMBOL_OBJECT;
                } else if (ELF32_ST_TYPE(sym.st_info) == STT_SECTION) {
                    symbol->type = SECTION_SYMBOL;
                }
                
                symbol->value = sym.st_value;
                symbol->size = sym.st_size;
                symbol->section_index = sym.st_shndx;
                symbol->binding = ELF32_ST_BIND(sym.st_info);
                symbol->visibility = sym.st_other;
            }
            
            break;
        }
    }
    
    return 0;
}

int resolve_symbols(LinkerState *state) {
    // First pass: collect all symbols
    for (int i = 0; i < state->num_objects; i++) {
        ObjectFile *obj = &state->objects[i];
        
        for (int j = 0; j < obj->num_symbols; j++) {
            Symbol *symbol = &obj->symbols[j];
            
            if (symbol->type != SYMBOL_UNDEF && symbol->binding == STB_GLOBAL) {
                // Add to output symbols
                if (state->num_output_symbols < MAX_SYMBOLS) {
                    Symbol *output_symbol = &state->output_symbols[state->num_output_symbols];
                    *output_symbol = *symbol;
                    state->num_output_symbols++;
                }
            }
        }
    }
    
    // Second pass: resolve undefined symbols
    int resolved = 0;
    for (int i = 0; i < state->num_objects; i++) {
        ObjectFile *obj = &state->objects[i];
        
        for (int j = 0; j < obj->num_symbols; j++) {
            Symbol *symbol = &obj->symbols[j];
            
            if (symbol->type == SYMBOL_UNDEF) {
                // Look for definition in other objects
                for (int k = 0; k < state->num_output_symbols; k++) {
                    Symbol *output_symbol = &state->output_symbols[k];
                    
                    if (strcmp(symbol->name, output_symbol->name) == 0) {
                        // Found definition
                        symbol->type = output_symbol->type;
                        symbol->value = output_symbol->value;
                        symbol->size = output_symbol->size;
                        symbol->section_index = output_symbol->section_index;
                        resolved++;
                        break;
                    }
                }
            }
        }
    }
    
    if (state->verbose) {
        printf("Resolved %d symbols\n", resolved);
    }
    
    return 0;
}

int merge_sections(LinkerState *state) {
    // Merge sections of the same type
    for (int i = 0; i < state->num_objects; i++) {
        ObjectFile *obj = &state->objects[i];
        
        for (int j = 0; j < obj->num_sections; j++) {
            Section *section = &obj->sections[j];
            
            // Skip debug sections if requested
            if (state->strip_debug && strcmp(section->name, ".debug_info") == 0) {
                continue;
            }
            
            // Find matching output section
            int output_idx = -1;
            for (int k = 0; k < state->num_output_sections; k++) {
                if (strcmp(state->output_sections[k].name, section->name) == 0) {
                    output_idx = k;
                    break;
                }
            }
            
            if (output_idx < 0) {
                // Create new output section
                if (state->num_output_sections < MAX_SECTIONS) {
                    Section *output_section = &state->output_sections[state->num_output_sections];
                    *output_section = *section;
                    state->num_output_sections++;
                }
            } else {
                // Merge with existing section
                Section *output_section = &state->output_sections[output_idx];
                output_section->size += section->size;
                
                // Copy data
                if (section->data && section->type != SECTION_BSS) {
                    unsigned char *new_data = realloc(output_section->data, output_section->size);
                    if (!new_data) {
                        fprintf(stderr, "kenux-ld: out of memory\n");
                        return -1;
                    }
                    output_section->data = new_data;
                    memcpy(output_section->data + (output_section->size - section->size), 
                           section->data, section->size);
                }
            }
        }
    }
    
    if (state->verbose) {
        printf("Merged %d sections\n", state->num_output_sections);
    }
    
    return 0;
}

int create_output_file(LinkerState *state) {
    state->output_fd = open(state->output_file, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (state->output_fd < 0) {
        fprintf(stderr, "kenux-ld: cannot create output file: %s\n", state->output_file);
        return -1;
    }
    
    if (state->verbose) {
        printf("Created output file: %s\n", state->output_file);
    }
    
    return 0;
}

int write_output_sections(LinkerState *state) {
    // Write section data to output file
    for (int i = 0; i < state->num_output_sections; i++) {
        Section *section = &state->output_sections[i];
        
        if (section->data && section->size > 0) {
            if (write(state->output_fd, section->data, section->size) != section->size) {
                fprintf(stderr, "kenux-ld: failed to write section data\n");
                return -1;
            }
        }
    }
    
    return 0;
}

int write_output_symbols(LinkerState *state) {
    // Write symbol table to output file (simplified)
    // In a real implementation, this would write proper ELF symbol table
    
    if (state->verbose) {
        printf("Wrote %d symbols to output\n", state->num_output_symbols);
    }
    
    return 0;
}

int relocate_symbols(LinkerState *state) {
    // Relocate symbols based on section addresses
    // This is a simplified implementation
    
    size_t current_addr = 0;
    
    for (int i = 0; i < state->num_output_sections; i++) {
        Section *section = &state->output_sections[i];
        
        // Set virtual address
        section->virtual_address = current_addr;
        current_addr += section->size;
        
        // Align to section alignment
        if (section->alignment > 1) {
            current_addr = (current_addr + section->alignment - 1) & ~(section->alignment - 1);
        }
    }
    
    // Update symbol values
    for (int i = 0; i < state->num_output_symbols; i++) {
        Symbol *symbol = &state->output_symbols[i];
        
        if (symbol->section_index > 0 && symbol->section_index <= state->num_output_sections) {
            Section *section = &state->output_sections[symbol->section_index - 1];
            symbol->value = section->virtual_address + symbol->value;
        }
    }
    
    if (state->verbose) {
        printf("Relocated symbols\n");
    }
    
    return 0;
}

void print_linker_state(LinkerState *state) {
    printf("Linker State:\n");
    printf("  Output file: %s\n", state->output_file);
    printf("  Entry point: %s\n", state->entry_point);
    printf("  Number of object files: %d\n", state->num_objects);
    printf("  Number of output sections: %d\n", state->num_output_sections);
    printf("  Number of output symbols: %d\n", state->num_output_symbols);
    printf("  Verbose: %d\n", state->verbose);
    printf("  Strip debug: %d\n", state->strip_debug);
    printf("  Create shared: %d\n", state->create_shared);
    printf("  PIE enabled: %d\n", state->pie_enabled);
}

void print_usage(void) {
    printf("Kenux OS Linker\n");
    printf("Usage: ld [options] file...\n");
    printf("Options:\n");
    printf("  -o file       Output file (default: a.out)\n");
    printf("  -e addr       Entry point address\n");
    printf("  -v            Verbose output\n");
    printf("  --strip-debug  Strip debug information\n");
    printf("  --shared      Create shared library\n");
    printf("  -pie          Enable position-independent executable\n");
    printf("  --help        Show this help message\n");
}

int main(int argc, char **argv) {
    LinkerState state;
    linker_init(&state);
    
    // Parse arguments
    if (parse_arguments(&state, argc, argv) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (state.num_objects == 0) {
        fprintf(stderr, "kenux-ld: no input files\n");
        print_usage();
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Link process
    if (resolve_symbols(&state) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (merge_sections(&state) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (relocate_symbols(&state) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (create_output_file(&state) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (write_output_sections(&state) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (write_output_symbols(&state) != 0) {
        linker_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    if (state.verbose) {
        print_linker_state(&state);
    }
    
    linker_cleanup(&state);
    return EXIT_SUCCESS;
}