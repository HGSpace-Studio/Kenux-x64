

#ifndef KAPI_PERCPU_H
#define KAPI_PERCPU_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_PERCPU_MAX_CPUS  16

typedef struct kapi_percpu_area {
    void*   data;
    size_t  size;
    uint8_t in_use;
} kapi_percpu_area_t;

int  kapi_percpu_init(void);

/* 分配一个 per-CPU 变量区域，返回 area_id；返回 -1 表示失败 */
int  kapi_percpu_alloc(size_t size);

/* 释放 per-CPU 变量 */
void kapi_percpu_free(int area_id);

/* 获取当前 CPU 的 per-CPU 变量指针 */
void* kapi_percpu_ptr(int area_id);

/* 获取指定 CPU 的 per-CPU 变量指针 */
void* kapi_percpu_ptr_on_cpu(int area_id, int cpu);

/* 获取当前 CPU id（与 kapi_smp_processor_id 一致） */
int  kapi_percpu_cpu_id(void);

/* 通用 per-CPU 加法 */
void kapi_percpu_add(int area_id, int64_t delta);

/* 通用 per-CPU 读取 */
int64_t kapi_percpu_read(int area_id);

/* 通用 per-CPU 写入 */
void kapi_percpu_write(int area_id, int64_t value);

/* 用于 DEFINE_PER_CPU 静态分配 */
#define KAPI_DEFINE_PER_CPU(type, name) \
    static type __percpu_##name[KAPI_PERCPU_MAX_CPUS]; \
    static int  __percpu_##name##_id = -1

#define KAPI_PER_CPU(name, cpu) \
    (&(__percpu_##name[(cpu) & (KAPI_PERCPU_MAX_CPUS - 1)]))

#define KAPI_THIS_CPU(name) \
    KAPI_PER_CPU(name, kapi_percpu_cpu_id())

#define KAPI_PER_CPU_INIT(name) \
    do { \
        __percpu_##name##_id = kapi_percpu_alloc(sizeof(type)); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
