#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned long size_t;
typedef signed long ssize_t;
typedef int ptrdiff_t;
typedef long wchar_t;

#define NULL ((void*)0)
#define offsetof(type, member) ((size_t)&((type*)0)->member)

#endif