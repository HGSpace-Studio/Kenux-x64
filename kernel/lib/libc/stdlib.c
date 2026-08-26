#include <arch/mmap.h>
#include <string.h>
#include <kapi_syscall.h>

/* 内联汇编 syscall 辅助函数 */
static inline long syscall1(long n, long a1)
{
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx","r11","memory");
    return ret;
}

/* 全局 mmap 上下文（libc 内部使用） */
static mmap_context_t libc_mmap_ctx;
static int libc_mmap_inited = 0;

static void libc_mmap_init(void)
{
    if (!libc_mmap_inited) {
        mmap_context_init(&libc_mmap_ctx);
        libc_mmap_inited = 1;
    }
}

/* 分配表（最多 256 个条目） */
#define ALLOC_TABLE_MAX 256
typedef struct {
    void*  addr;
    size_t size;
    int    active;
} alloc_entry_t;

static alloc_entry_t alloc_table[ALLOC_TABLE_MAX];
static int alloc_table_inited = 0;

static void alloc_table_init(void)
{
    if (!alloc_table_inited) {
        for (int i = 0; i < ALLOC_TABLE_MAX; i++) {
            alloc_table[i].active = 0;
        }
        alloc_table_inited = 1;
    }
}

static int alloc_table_insert(void* addr, size_t size)
{
    for (int i = 0; i < ALLOC_TABLE_MAX; i++) {
        if (!alloc_table[i].active) {
            alloc_table[i].addr   = addr;
            alloc_table[i].size   = size;
            alloc_table[i].active = 1;
            return i;
        }
    }
    return -1;
}

static int alloc_table_find(void* addr)
{
    for (int i = 0; i < ALLOC_TABLE_MAX; i++) {
        if (alloc_table[i].active && alloc_table[i].addr == addr) {
            return i;
        }
    }
    return -1;
}

static void alloc_table_remove(int idx)
{
    if (idx >= 0 && idx < ALLOC_TABLE_MAX) {
        alloc_table[idx].active = 0;
        alloc_table[idx].addr   = NULL;
        alloc_table[idx].size   = 0;
    }
}

/* 分配匿名内存 */
void* malloc(size_t size)
{
    if (size == 0) return NULL;

    alloc_table_init();
    libc_mmap_init();

    uint64_t addr = mmap_do_mmap(&libc_mmap_ctx, 0, size,
                                 PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS, -1, 0);
    if (addr == (uint64_t)-1 || addr == 0) {
        return NULL;
    }

    if (alloc_table_insert((void*)addr, size) < 0) {
        mmap_do_munmap(&libc_mmap_ctx, addr, size);
        return NULL;
    }
    return (void*)addr;
}

/* 释放内存 */
void free(void* ptr)
{
    if (!ptr) return;
    alloc_table_init();

    int idx = alloc_table_find(ptr);
    if (idx < 0) {
        /* 未找到的指针，安全忽略 */
        return;
    }

    size_t size = alloc_table[idx].size;
    alloc_table_remove(idx);

    libc_mmap_init();
    mmap_do_munmap(&libc_mmap_ctx, (uint64_t)ptr, size);
}

/* 清零分配 */
void* calloc(size_t num, size_t size)
{
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/* 重新分配 */
void* realloc(void* ptr, size_t size)
{
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    int idx = alloc_table_find(ptr);
    if (idx < 0) {
        /* 非法指针，返回 NULL */
        return NULL;
    }

    size_t old_size = alloc_table[idx].size;
    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    size_t copy_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
    return new_ptr;
}

/* 退出函数数组（最多 32 个） */
#define ATEXIT_MAX 32
static void (*atexit_funcs[ATEXIT_MAX])(void);
static int atexit_count = 0;

int atexit(void (*func)(void))
{
    if (!func) return -1;
    if (atexit_count >= ATEXIT_MAX) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

/* 进程退出 */
void exit(int status)
{
    /* 逆序调用注册的退出函数 */
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_funcs[i]) {
            atexit_funcs[i]();
        }
    }
    syscall1(SYS_exit, status);
    while (1); /* 永不返回 */
}

/* 绝对值 */
int abs(int n)
{
    return (n < 0) ? -n : n;
}

/* long 绝对值 */
long labs(long n)
{
    return (n < 0) ? -n : n;
}

/* long long 绝对值 */
long long llabs(long long n)
{
    return (n < 0) ? -n : n;
}

/* 异常终止 */
void abort(void)
{
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* 字符串转整数 */
int atoi(const char* str)
{
    if (!str) return 0;
    int sign = 1;
    int result = 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' ||
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return sign * result;
}

/* 字符串转 long */
long atol(const char* str)
{
    if (!str) return 0;
    int sign = 1;
    long result = 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' ||
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return sign * result;
}

/* 字符串转 long long */
long long atoll(const char* str)
{
    if (!str) return 0;
    int sign = 1;
    long long result = 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' ||
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return sign * result;
}

/* 完整 strtol 实现 */
long strtol(const char* str, char** endptr, int base)
{
    if (!str) {
        if (endptr) *endptr = NULL;
        return 0;
    }
    const char* s = str;
    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\f' || *s == '\v') {
        s++;
    }

    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    if ((base == 0 || base == 16) && *s == '0' &&
        (*(s + 1) == 'x' || *(s + 1) == 'X')) {
        base = 16;
        s += 2;
    } else if (base == 0 && *s == '0') {
        base = 8;
        s++;
    } else if (base == 0) {
        base = 10;
    }

    long result = 0;
    int valid = 0;
    while (1) {
        int digit = -1;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;

        if (digit < 0 || digit >= base) break;
        valid = 1;
        result = result * base + digit;
        s++;
    }

    if (endptr) {
        *endptr = valid ? (char*)s : (char*)str;
    }
    return sign * result;
}

/* 伪随机数生成器（线性同余） */
static unsigned int rand_seed = 1;

int rand(void)
{
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)(rand_seed & 0x7FFF);
}

void srand(unsigned int seed)
{
    rand_seed = seed;
}
