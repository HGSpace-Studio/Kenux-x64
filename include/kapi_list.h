

#ifndef KAPI_LIST_H
#define KAPI_LIST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_list_head {
    struct kapi_list_head* next;
    struct kapi_list_head* prev;
} kapi_list_head_t;

#define KAPI_LIST_HEAD_INIT(name) { &(name), &(name) }

#define KAPI_LIST_HEAD(name) \
    kapi_list_head_t name = KAPI_LIST_HEAD_INIT(name)

#define kapi_list_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

#define kapi_list_first_entry(ptr, type, member) \
    kapi_list_entry((ptr)->next, type, member)

#define kapi_list_next_entry(pos, type, member) \
    kapi_list_entry((pos)->member.next, type, member)

#define kapi_list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define kapi_list_for_each_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; \
         (pos) != (head); \
         (pos) = (n), (n) = (pos)->next)

static inline void kapi_list_init(kapi_list_head_t* head)
{
    head->next = head;
    head->prev = head;
}

static inline void __kapi_list_add(kapi_list_head_t* new,
                                    kapi_list_head_t* prev,
                                    kapi_list_head_t* next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void kapi_list_add(kapi_list_head_t* new, kapi_list_head_t* head)
{
    __kapi_list_add(new, head, head->next);
}

static inline void kapi_list_add_tail(kapi_list_head_t* new, kapi_list_head_t* head)
{
    __kapi_list_add(new, head->prev, head);
}

static inline void __kapi_list_del(kapi_list_head_t* prev, kapi_list_head_t* next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void kapi_list_del(kapi_list_head_t* entry)
{
    __kapi_list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

static inline int kapi_list_empty(const kapi_list_head_t* head)
{
    return head->next == head;
}

#ifdef __cplusplus
}
#endif

#endif
