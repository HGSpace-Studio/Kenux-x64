/*
 * Kenux OS - Data Duplication Tool
 * Header file for dd functionality
 */

#ifndef _DD_H
#define _DD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

// Maximum path length
#define MAX_PATH_LEN 4096

// Conversion options
typedef enum {
    CONV_NONE,
    CONV_ASCII,
    CONV_EBCDIC,
    CONV_BLOCK,
    CONV_UNBLOCK,
    CONV_LCASE,
    CONV_UCASE,
    CONV_SWAB,
    CONV_NOERROR,
    CONV_NOTRUNC,
    CONV_SYNC,
    CONV_FSYNC,
    CONV_EXCL
} ConversionMode;

// I/O status
typedef struct {
    size_t bytes_read;
    size_t bytes_written;
    size_t records_in;
    size_t records_out;
    time_t start_time;
    time_t end_time;
    double transfer_rate;
} IOStatus;

// dd state
typedef struct {
    int ifd;           // Input file descriptor
    int ofd;           // Output file descriptor
    char input_file[MAX_PATH_LEN];
    char output_file[MAX_PATH_LEN];
    char input_if[MAX_PATH_LEN];
    char output_of[MAX_PATH_LEN];
    size_t ibs;         // Input block size
    size_t obs;         // Output block size
    size_t cbs;         // Conversion block size
    size_t skip;        // Skip blocks
    size_t seek;        // Seek blocks
    size_t count;       // Copy N input blocks
    ConversionMode conv;
    int status;
    int sparse;
    int nocreat;
    int excl;
    int notrunc;
    int sync;
    int fdatasync;
    int noerror;
    int progress;
    IOStatus io_status;
} DdState;

// Function prototypes
void dd_init(DdState *state);
void dd_cleanup(DdState *state);
int parse_arguments(DdState *state, int argc, char **argv);
int open_input_file(DdState *state);
int open_output_file(DdState *state);
int process_data(DdState *state);
void apply_conversions(DdState *state, char *data, size_t size);
void update_io_status(DdState *state);
void print_progress(DdState *state);
void print_final_stats(DdState *state);
void print_usage(void);
const char *get_conv_name(ConversionMode mode);

#endif /* _DD_H */