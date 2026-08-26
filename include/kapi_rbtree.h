

#ifndef KAPI_RBTREE_H
#define KAPI_RBTREE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_RB_RED   0
#define KAPI_RB_BLACK 1

typedef struct kapi_rb_node {
    struct kapi_rb_node* parent;
    struct kapi_rb_node* right;
    struct kapi_rb_node* left;
    int color;
} kapi_rb_node_t;

typedef struct {
    kapi_rb_node_t* root;
} kapi_rb_root_t;

#define KAPI_RB_ROOT (kapi_rb_root_t){ NULL }

#define kapi_rb_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

void kapi_rb_insert(kapi_rb_node_t* node, kapi_rb_root_t* root,
                    int (*cmp)(const kapi_rb_node_t*, const kapi_rb_node_t*));

void kapi_rb_erase(kapi_rb_node_t* node, kapi_rb_root_t* root);

kapi_rb_node_t* kapi_rb_find(const kapi_rb_root_t* root, const kapi_rb_node_t* key,
                             int (*cmp)(const kapi_rb_node_t*, const kapi_rb_node_t*));

kapi_rb_node_t* kapi_rb_first(const kapi_rb_root_t* root);

kapi_rb_node_t* kapi_rb_next(const kapi_rb_node_t* node);

#ifdef __cplusplus
}
#endif

#endif
