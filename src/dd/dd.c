/*
 * Kenux OS - Data Duplication Tool
 * Main dd functionality
 */

#include "dd.h"

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#else
/* Windows MinGW compatibility */
#include <io.h>
#include <process.h>
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE 1
#endif

/* fsync stub: use commit/flush on Windows via _commit */
static inline int kenux_fsync(int fd) {
    return _commit(fd);
}
#define fsync kenux_fsync
#endif

// Default values
#define DEFAULT_IBS 512
#define DEFAULT_OBS 512
#define DEFAULT_CBS 512

void dd_init(DdState *state) {
    memset(state, 0, sizeof(DdState));
    state->ibs = DEFAULT_IBS;
    state->obs = DEFAULT_OBS;
    state->cbs = DEFAULT_CBS;
    state->conv = CONV_NONE;
    state->sparse = 0;
    state->nocreat = 0;
    state->excl = 0;
    state->notrunc = 0;
    state->sync = 0;
    state->fdatasync = 0;
    state->noerror = 0;
    state->progress = 0;
    state->io_status.start_time = time(NULL);
}

void dd_cleanup(DdState *state) {
    if (state->ifd >= 0) {
        close(state->ifd);
    }
    if (state->ofd >= 0) {
        close(state->ofd);
    }
    state->io_status.end_time = time(NULL);
}

int parse_arguments(DdState *state, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "if=", 3) == 0) {
            strncpy(state->input_file, argv[i] + 3, MAX_PATH_LEN - 1);
            state->input_file[MAX_PATH_LEN - 1] = '\0';
        } else if (strncmp(argv[i], "of=", 3) == 0) {
            strncpy(state->output_file, argv[i] + 3, MAX_PATH_LEN - 1);
            state->output_file[MAX_PATH_LEN - 1] = '\0';
        } else if (strncmp(argv[i], "ibs=", 4) == 0) {
            state->ibs = atoi(argv[i] + 4);
        } else if (strncmp(argv[i], "obs=", 4) == 0) {
            state->obs = atoi(argv[i] + 4);
        } else if (strncmp(argv[i], "cbs=", 4) == 0) {
            state->cbs = atoi(argv[i] + 4);
        } else if (strncmp(argv[i], "skip=", 5) == 0) {
            state->skip = atoi(argv[i] + 5);
        } else if (strncmp(argv[i], "seek=", 5) == 0) {
            state->seek = atoi(argv[i] + 5);
        } else if (strncmp(argv[i], "count=", 6) == 0) {
            state->count = atoi(argv[i] + 6);
        } else if (strcmp(argv[i], "conv=ascii") == 0) {
            state->conv = CONV_ASCII;
        } else if (strcmp(argv[i], "conv=ebcdic") == 0) {
            state->conv = CONV_EBCDIC;
        } else if (strcmp(argv[i], "conv=block") == 0) {
            state->conv = CONV_BLOCK;
        } else if (strcmp(argv[i], "conv=unblock") == 0) {
            state->conv = CONV_UNBLOCK;
        } else if (strcmp(argv[i], "conv=lcase") == 0) {
            state->conv = CONV_LCASE;
        } else if (strcmp(argv[i], "conv=ucase") == 0) {
            state->conv = CONV_UCASE;
        } else if (strcmp(argv[i], "conv=swab") == 0) {
            state->conv = CONV_SWAB;
        } else if (strcmp(argv[i], "conv=noerror") == 0) {
            state->conv = CONV_NOERROR;
            state->noerror = 1;
        } else if (strcmp(argv[i], "conv=notrunc") == 0) {
            state->conv = CONV_NOTRUNC;
            state->notrunc = 1;
        } else if (strcmp(argv[i], "conv=sync") == 0) {
            state->conv = CONV_SYNC;
            state->sync = 1;
        } else if (strcmp(argv[i], "conv=fsync") == 0) {
            state->conv = CONV_FSYNC;
            state->fdatasync = 1;
        } else if (strcmp(argv[i], "conv=excl") == 0) {
            state->conv = CONV_EXCL;
            state->excl = 1;
        } else if (strcmp(argv[i], "sparse") == 0) {
            state->sparse = 1;
        } else if (strcmp(argv[i], "nocreat") == 0) {
            state->nocreat = 1;
        } else if (strcmp(argv[i], "status=progress") == 0) {
            state->progress = 1;
        } else if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "version") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("kenux-dd Kenux OS Data Duplication Tool\n");
            return 0;
        } else {
            fprintf(stderr, "kenux-dd: unknown operand: %s\n", argv[i]);
            return -1;
        }
    }
    
    // Check for required arguments
    if (strlen(state->input_file) == 0) {
        // Default to stdin
        strncpy(state->input_file, "-", MAX_PATH_LEN - 1);
    }
    
    if (strlen(state->output_file) == 0) {
        // Default to stdout
        strncpy(state->output_file, "-", MAX_PATH_LEN - 1);
    }
    
    return 0;
}

int open_input_file(DdState *state) {
    if (strcmp(state->input_file, "-") == 0) {
        state->ifd = STDIN_FILENO;
    } else {
        state->ifd = open(state->input_file, O_RDONLY);
        if (state->ifd < 0) {
            fprintf(stderr, "kenux-dd: cannot open input file: %s\n", state->input_file);
            return -1;
        }
    }
    
    // Skip N blocks if requested
    for (size_t i = 0; i < state->skip; i++) {
        char skip_buf[state->ibs];
        ssize_t bytes_read = read(state->ifd, skip_buf, sizeof(skip_buf));
        if (bytes_read < 0) {
            fprintf(stderr, "kenux-dd: error skipping input blocks\n");
            return -1;
        }
        if (bytes_read == 0) {
            fprintf(stderr, "kenux-dd: reached end of file while skipping\n");
            return -1;
        }
    }
    
    return 0;
}

int open_output_file(DdState *state) {
    int oflags = O_WRONLY | O_CREAT;
    
    if (state->excl) {
        oflags |= O_EXCL;
    }
    
    if (strcmp(state->output_file, "-") == 0) {
        state->ofd = STDOUT_FILENO;
    } else {
        state->ofd = open(state->output_file, oflags, 0644);
        if (state->ofd < 0) {
            fprintf(stderr, "kenux-dd: cannot open output file: %s\n", state->output_file);
            return -1;
        }
    }
    
    // Seek N blocks if requested
    for (size_t i = 0; i < state->seek; i++) {
        if (lseek(state->ofd, state->obs, SEEK_CUR) == -1) {
            fprintf(stderr, "kenux-dd: error seeking output file\n");
            return -1;
        }
    }
    
    return 0;
}

void apply_conversions(DdState *state, char *data, size_t size) {
    switch (state->conv) {
        case CONV_ASCII:
            // Convert EBCDIC to ASCII
            for (size_t i = 0; i < size; i++) {
                if (data[i] >= 0x40 && data[i] <= 0xFE) {
                    data[i] -= 0x40;
                }
            }
            break;
            
        case CONV_EBCDIC:
            // Convert ASCII to EBCDIC
            for (size_t i = 0; i < size; i++) {
                if (data[i] >= 0x20 && data[i] <= 0x7E) {
                    data[i] += 0x40;
                }
            }
            break;
            
        case CONV_LCASE:
            // Convert to lowercase
            for (size_t i = 0; i < size; i++) {
                if (data[i] >= 'A' && data[i] <= 'Z') {
                    data[i] += 32;
                }
            }
            break;
            
        case CONV_UCASE:
            // Convert to uppercase
            for (size_t i = 0; i < size; i++) {
                if (data[i] >= 'a' && data[i] <= 'z') {
                    data[i] -= 32;
                }
            }
            break;
            
        case CONV_SWAB:
            // Swap bytes
            for (size_t i = 0; i < size - 1; i += 2) {
                char temp = data[i];
                data[i] = data[i + 1];
                data[i + 1] = temp;
            }
            break;
            
        case CONV_SYNC:
            // Fill with zeros
            memset(data, 0, state->cbs);
            break;
            
        case CONV_BLOCK:
            // Pad to cbs
            if (size < state->cbs) {
                memset(data + size, 0, state->cbs - size);
                size = state->cbs;
            }
            break;
            
        case CONV_UNBLOCK:
            // Remove trailing spaces
            while (size > 0 && data[size - 1] == ' ') {
                size--;
            }
            break;
            
        default:
            break;
    }
}

int process_data(DdState *state) {
    char *input_buffer = malloc(state->ibs);
    char *output_buffer = malloc(state->obs);
    
    if (!input_buffer || !output_buffer) {
        fprintf(stderr, "kenux-dd: out of memory\n");
        free(input_buffer);
        free(output_buffer);
        return -1;
    }
    
    size_t blocks_processed = 0;
    
    while (1) {
        // Read input
        ssize_t bytes_read = read(state->ifd, input_buffer, state->ibs);
        
        if (bytes_read < 0) {
            if (!state->noerror) {
                fprintf(stderr, "kenux-dd: read error: %s\n", strerror(errno));
                break;
            } else {
                // Skip error
                bytes_read = 0;
            }
        } else if (bytes_read == 0) {
            // End of file
            break;
        }
        
        // Check count limit
        if (state->count > 0 && blocks_processed >= state->count) {
            break;
        }
        
        // Apply conversions
        apply_conversions(state, input_buffer, bytes_read);
        
        // Write output
        ssize_t bytes_written = write(state->ofd, input_buffer, bytes_read);
        
        if (bytes_written < 0) {
            fprintf(stderr, "kenux-dd: write error: %s\n", strerror(errno));
            if (!state->notrunc) {
                break;
            }
        }
        
        // Update status
        state->io_status.bytes_read += bytes_read;
        state->io_status.bytes_written += bytes_written;
        state->io_status.records_in++;
        state->io_status.records_out++;
        blocks_processed++;
        
        // Print progress if requested
        if (state->progress && (blocks_processed % 1000 == 0)) {
            print_progress(state);
        }
        
        // Flush if requested
        if (state->fdatasync) {
            fsync(state->ofd);
        }
    }
    
    // Final progress update
    if (state->progress) {
        print_progress(state);
    }
    
    free(input_buffer);
    free(output_buffer);
    return 0;
}

void update_io_status(DdState *state) {
    time_t end_time = time(NULL);
    double elapsed = difftime(end_time, state->io_status.start_time);
    
    if (elapsed > 0) {
        state->io_status.transfer_rate = state->io_status.bytes_read / elapsed;
    }
}

void print_progress(DdState *state) {
    update_io_status(state);
    
    double mb_read = state->io_status.bytes_read / (1024.0 * 1024.0);
    double mb_written = state->io_status.bytes_written / (1024.0 * 1024.0);
    
    printf("\r%8.2fMB in, %8.2fMB out", mb_read, mb_written);
    fflush(stdout);
}

void print_final_stats(DdState *state) {
    update_io_status(state);
    
    printf("\n");
    printf("%zu+%zu records in\n", state->io_status.records_in, state->io_status.records_in);
    printf("%zu+%zu records out\n", state->io_status.records_out, state->io_status.records_out);
    printf("%zu bytes (%.2fMB) copied, %.2f s, %.2f kB/s\n", 
           state->io_status.bytes_read, 
           state->io_status.bytes_read / (1024.0 * 1024.0),
           difftime(state->io_status.end_time, state->io_status.start_time),
           state->io_status.transfer_rate / 1024.0);
}

void print_usage(void) {
    printf("Kenux OS Data Duplication Tool\n");
    printf("Usage: dd [OPERAND]...\n");
    printf("Copy a file, converting and formatting according to the operands.\n\n");
    printf("OPERANDS:\n");
    printf("  if=FILE       read from FILE instead of stdin\n");
    printf("  of=FILE       write to FILE instead of stdout\n");
    printf("  ibs=N         read N bytes at a time (default: %d)\n", DEFAULT_IBS);
    printf("  obs=N         write N bytes at a time (default: %d)\n", DEFAULT_OBS);
    printf("  cbs=N         convert N bytes at a time\n");
    printf("  skip=N        skip N blocks from input\n");
    printf("  seek=N        skip N blocks from output\n");
    printf("  count=N       copy only N input blocks\n");
    printf("  conv=CONV     convert the file as per the comma separated symbol list\n");
    printf("  sparse       create sparse output file\n");
    printf("  nocreat      do not create the output file\n");
    printf("  status=progress   show periodic transfer statistics\n");
    printf("\n");
    printf("CONVERSION symbols:\n");
    printf("  ascii     convert EBCDIC to ASCII\n");
    printf("  ebcdic    convert ASCII to EBCDIC\n");
    printf("  block     pad newline-terminated records with spaces\n");
    printf("  unblock   replace trailing spaces in newline-terminated records with newline\n");
    printf("  lcase     change uppercase to lowercase\n");
    printf("  ucase     change lowercase to uppercase\n");
    printf("  swab      swap every pair of bytes\n");
    printf("  noerror   continue after read errors\n");
    printf("  notrunc   do not truncate the output file\n");
    printf("  sync      pad every input block with NULs\n");
    printf("  fsync     physically write output file data before finishing\n");
    printf("  excl      fail if the output file already exists\n");
    printf("\n");
    printf("Examples:\n");
    printf("  dd if=/dev/zero of=zeroes bs=1M count=100\n");
    printf("  dd if=/dev/urandom of=random.dat bs=1024 count=4096 conv=swab\n");
}

int main(int argc, char **argv) {
    DdState state;
    dd_init(&state);
    
    // Parse arguments
    if (parse_arguments(&state, argc, argv) != 0) {
        dd_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Open input file
    if (open_input_file(&state) != 0) {
        dd_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Open output file
    if (open_output_file(&state) != 0) {
        dd_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Process data
    if (process_data(&state) != 0) {
        dd_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Print final statistics
    if (state.progress || (state.io_status.bytes_read > 0)) {
        print_final_stats(&state);
    }
    
    dd_cleanup(&state);
    return EXIT_SUCCESS;
}