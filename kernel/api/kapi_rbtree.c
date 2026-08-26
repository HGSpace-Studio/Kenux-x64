

#include "kapi_rbtree.h"
#include "kapi.h"

static inline void kapi_rb_set_parent(kapi_rb_node_t* node, kapi_rb_node_t* parent)
{
    node->parent = parent;
}

static inline int kapi_rb_color(const kapi_rb_node_t* node)
{
    return node->color;
}

static inline void kapi_rb_set_color(kapi_rb_node_t* node, int color)
{
    node->color = color;
}

static void __kapi_rb_rotate_left(kapi_rb_node_t* node, kapi_rb_root_t* root)
{
    kapi_rb_node_t* right = node->right;
    kapi_rb_node_t* parent = node->parent;

    if ((node->right = right->left)) {
        right->left->parent = node;
    }
    right->left = node;
    right->parent = parent;

    if (parent) {
        if (node == parent->left) {
            parent->left = right;
        } else {
            parent->right = right;
        }
    } else {
        root->root = right;
    }
    node->parent = right;
}

static void __kapi_rb_rotate_right(kapi_rb_node_t* node, kapi_rb_root_t* root)
{
    kapi_rb_node_t* left = node->left;
    kapi_rb_node_t* parent = node->parent;

    if ((node->left = left->right)) {
        left->right->parent = node;
    }
    left->right = node;
    left->parent = parent;

    if (parent) {
        if (node == parent->right) {
            parent->right = left;
        } else {
            parent->left = left;
        }
    } else {
        root->root = left;
    }
    node->parent = left;
}

void kapi_rb_insert(kapi_rb_node_t* node, kapi_rb_root_t* root,
                    int (*cmp)(const kapi_rb_node_t*, const kapi_rb_node_t*))
{
    kapi_rb_node_t* parent = NULL;
    kapi_rb_node_t** link = &root->root;

    while (*link) {
        parent = *link;
        if (cmp(node, parent) < 0) {
            link = &parent->left;
        } else {
            link = &parent->right;
        }
    }

    node->left = NULL;
    node->right = NULL;
    node->parent = parent;
    node->color = KAPI_RB_RED;
    *link = node;

    while (parent && kapi_rb_color(parent) == KAPI_RB_RED) {
        kapi_rb_node_t* gparent = parent->parent;

        if (parent == gparent->left) {
            kapi_rb_node_t* uncle = gparent->right;
            if (uncle && kapi_rb_color(uncle) == KAPI_RB_RED) {
                kapi_rb_set_color(uncle, KAPI_RB_BLACK);
                kapi_rb_set_color(parent, KAPI_RB_BLACK);
                kapi_rb_set_color(gparent, KAPI_RB_RED);
                node = gparent;
                parent = node->parent;
                continue;
            }

            if (node == parent->right) {
                kapi_rb_node_t* tmp;
                __kapi_rb_rotate_left(parent, root);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            kapi_rb_set_color(parent, KAPI_RB_BLACK);
            kapi_rb_set_color(gparent, KAPI_RB_RED);
            __kapi_rb_rotate_right(gparent, root);
        } else {
            kapi_rb_node_t* uncle = gparent->left;
            if (uncle && kapi_rb_color(uncle) == KAPI_RB_RED) {
                kapi_rb_set_color(uncle, KAPI_RB_BLACK);
                kapi_rb_set_color(parent, KAPI_RB_BLACK);
                kapi_rb_set_color(gparent, KAPI_RB_RED);
                node = gparent;
                parent = node->parent;
                continue;
            }

            if (node == parent->left) {
                kapi_rb_node_t* tmp;
                __kapi_rb_rotate_right(parent, root);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            kapi_rb_set_color(parent, KAPI_RB_BLACK);
            kapi_rb_set_color(gparent, KAPI_RB_RED);
            __kapi_rb_rotate_left(gparent, root);
        }
    }

    kapi_rb_set_color(root->root, KAPI_RB_BLACK);
}

void kapi_rb_erase(kapi_rb_node_t* node, kapi_rb_root_t* root)
{
    kapi_rb_node_t* child;
    kapi_rb_node_t* parent;
    int color;

    if (!node->left) {
        child = node->right;
    } else if (!node->right) {
        child = node->left;
    } else {
        kapi_rb_node_t* old = node;
        kapi_rb_node_t* left;

        node = node->right;
        while ((left = node->left) != NULL) {
            node = left;
        }

        child = node->right;
        parent = node->parent;
        color = node->color;

        if (child) {
            child->parent = parent;
        }
        if (parent) {
            if (parent->left == node) {
                parent->left = child;
            } else {
                parent->right = child;
            }
        } else {
            root->root = child;
        }

        if (node->parent == old) {
            parent = node;
        }

        node->parent = old->parent;
        node->color = old->color;
        node->right = old->right;
        node->left = old->left;

        if (old->parent) {
            if (old->parent->left == old) {
                old->parent->left = node;
            } else {
                old->parent->right = node;
            }
        } else {
            root->root = node;
        }

        old->left->parent = node;
        if (old->right) {
            old->right->parent = node;
        }
        goto color;
    }

    parent = node->parent;
    color = node->color;

    if (child) {
        child->parent = parent;
    }
    if (parent) {
        if (parent->left == node) {
            parent->left = child;
        } else {
            parent->right = child;
        }
    } else {
        root->root = child;
    }

color:
    if (color == KAPI_RB_BLACK) {
        while (child != root->root && (!child || kapi_rb_color(child) == KAPI_RB_BLACK)) {
            if (child == parent->left) {
                kapi_rb_node_t* sibling = parent->right;

                if (kapi_rb_color(sibling) == KAPI_RB_RED) {
                    kapi_rb_set_color(sibling, KAPI_RB_BLACK);
                    kapi_rb_set_color(parent, KAPI_RB_RED);
                    __kapi_rb_rotate_left(parent, root);
                    sibling = parent->right;
                }

                if ((!sibling->left || kapi_rb_color(sibling->left) == KAPI_RB_BLACK) &&
                    (!sibling->right || kapi_rb_color(sibling->right) == KAPI_RB_BLACK)) {
                    kapi_rb_set_color(sibling, KAPI_RB_RED);
                    child = parent;
                    parent = child->parent;
                } else {
                    if (!sibling->right || kapi_rb_color(sibling->right) == KAPI_RB_BLACK) {
                        kapi_rb_set_color(sibling->left, KAPI_RB_BLACK);
                        kapi_rb_set_color(sibling, KAPI_RB_RED);
                        __kapi_rb_rotate_right(sibling, root);
                        sibling = parent->right;
                    }
                    kapi_rb_set_color(sibling, kapi_rb_color(parent));
                    kapi_rb_set_color(parent, KAPI_RB_BLACK);
                    if (sibling->right) {
                        kapi_rb_set_color(sibling->right, KAPI_RB_BLACK);
                    }
                    __kapi_rb_rotate_left(parent, root);
                    child = root->root;
                    break;
                }
            } else {
                kapi_rb_node_t* sibling = parent->left;

                if (kapi_rb_color(sibling) == KAPI_RB_RED) {
                    kapi_rb_set_color(sibling, KAPI_RB_BLACK);
                    kapi_rb_set_color(parent, KAPI_RB_RED);
                    __kapi_rb_rotate_right(parent, root);
                    sibling = parent->left;
                }

                if ((!sibling->right || kapi_rb_color(sibling->right) == KAPI_RB_BLACK) &&
                    (!sibling->left || kapi_rb_color(sibling->left) == KAPI_RB_BLACK)) {
                    kapi_rb_set_color(sibling, KAPI_RB_RED);
                    child = parent;
                    parent = child->parent;
                } else {
                    if (!sibling->left || kapi_rb_color(sibling->left) == KAPI_RB_BLACK) {
                        kapi_rb_set_color(sibling->right, KAPI_RB_BLACK);
                        kapi_rb_set_color(sibling, KAPI_RB_RED);
                        __kapi_rb_rotate_left(sibling, root);
                        sibling = parent->left;
                    }
                    kapi_rb_set_color(sibling, kapi_rb_color(parent));
                    kapi_rb_set_color(parent, KAPI_RB_BLACK);
                    if (sibling->left) {
                        kapi_rb_set_color(sibling->left, KAPI_RB_BLACK);
                    }
                    __kapi_rb_rotate_right(parent, root);
                    child = root->root;
                    break;
                }
            }
        }
        if (child) {
            kapi_rb_set_color(child, KAPI_RB_BLACK);
        }
    }
}

kapi_rb_node_t* kapi_rb_find(const kapi_rb_root_t* root, const kapi_rb_node_t* key,
                             int (*cmp)(const kapi_rb_node_t*, const kapi_rb_node_t*))
{
    kapi_rb_node_t* node = root->root;

    while (node) {
        int c = cmp(key, node);
        if (c < 0) {
            node = node->left;
        } else if (c > 0) {
            node = node->right;
        } else {
            return node;
        }
    }
    return NULL;
}

kapi_rb_node_t* kapi_rb_first(const kapi_rb_root_t* root)
{
    kapi_rb_node_t* node = root->root;

    if (!node) {
        return NULL;
    }
    while (node->left) {
        node = node->left;
    }
    return node;
}

kapi_rb_node_t* kapi_rb_next(const kapi_rb_node_t* node)
{
    kapi_rb_node_t* next;

    if (node->right) {
        next = node->right;
        while (next->left) {
            next = next->left;
        }
        return next;
    }

    next = node->parent;
    while (next && node == next->right) {
        node = next;
        next = next->parent;
    }
    return next;
}
