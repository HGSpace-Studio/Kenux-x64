#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

typedef long FILE;

int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int scanf(const char* format, ...);
int sscanf(const char* str, const char* format, ...);

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int fgetc(FILE* stream);
int fputc(int c, FILE* stream);
char* fgets(char* s, int size, FILE* stream);
int fputs(const char* s, FILE* stream);
int fprintf(FILE* stream, const char* format, ...);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);

int putchar(int c);
int getchar(void);
int puts(const char* s);
char* gets(char* s);

void perror(const char* s);
int remove(const char* path);
int rename(const char* oldpath, const char* newpath);

FILE* stdin;
FILE* stdout;
FILE* stderr;

#endif