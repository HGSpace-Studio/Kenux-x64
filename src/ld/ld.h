/*
 * Kenux OS - Linker Implementation
 * Header file for ld functionality
 */

#ifndef _LD_H
#define _LD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// Maximum file path length
#define MAX_PATH_LEN 4096
#define MAX_SECTIONS 32
#define MAX_SYMBOLS 256
#define MAX_OBJECT_FILES 32

// Section types
typedef enum {
    SECTION_CODE,
    SECTION_DATA,
    SECTION_BSS,
    SECTION_RODATA,
    SECTION_UNDEF,
    SECTION_UNKNOWN
} SectionType;

// Section structure
typedef struct {
    char name[64];
    SectionType type;
    size_t size;
    size_t virtual_address;
    size_t file_offset;
    size_t alignment;
    unsigned char *data;
} Section;

// Symbol types
typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_OBJECT,
    SECTION_SYMBOL,
    SYMBOL_UNDEF,
    SYMBOL_UNKNOWN
} SymbolType;

// Symbol structure
typedef struct {
    char name[128];
    SymbolType type;
    size_t value;
    size_t size;
    int section_index;
    int binding; // GLOBAL, LOCAL, WEAK
    int visibility; // DEFAULT, HIDDEN
} Symbol;

// Object file structure
typedef struct {
    char filename[MAX_PATH_LEN];
    int fd;
    size_t file_size;
    size_t entry_point;
    int num_sections;
    Section sections[MAX_SECTIONS];
    int num_symbols;
    Symbol symbols[MAX_SYMBOLS];
} ObjectFile;

// Linker state
typedef struct {
    int output_fd;
    char output_file[MAX_PATH_LEN];
    char entry_point[MAX_PATH_LEN];
    int verbose;
    int strip_debug;
    int create_shared;
    int pie_enabled;
    ObjectFile objects[MAX_OBJECT_FILES];
    int num_objects;
    Section output_sections[MAX_SECTIONS];
    int num_output_sections;
    Symbol output_symbols[MAX_SYMBOLS];
    int num_output_symbols;
} LinkerState;

// Function prototypes
void linker_init(LinkerState *state);
void linker_cleanup(LinkerState *state);
int parse_arguments(LinkerState *state, int argc, char **argv);
int load_object_file(LinkerState *state, const char *filename);
int parse_elf_header(LinkerState *state, ObjectFile *obj);
int parse_section_headers(LinkerState *state, ObjectFile *obj);
int parse_symbol_table(LinkerState *state, ObjectFile *obj);
int resolve_symbols(LinkerState *state);
int merge_sections(LinkerState *state);
int create_output_file(LinkerState *state);
int write_output_sections(LinkerState *state);
int write_output_symbols(LinkerState *state);
int relocate_symbols(LinkerState *state);
void print_linker_state(LinkerState *state);
void print_usage(void);

#endif /* _LD_H */