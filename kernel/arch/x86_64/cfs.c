

#include <arch/cfs.h>
#include <string.h>

static const uint64_t nice_to_weight[40] = {
      88761, 71755, 56483, 46273, 36291,
      29154, 23254, 18705, 14949, 11916,
      9548,  7620,  6100,  4904,  3906,
      3121,  2501,  1991,  1586,  1277,
      1024,  820,   655,   526,   423,
      335,   272,   215,   172,   137,
      110,   87,    70,    56,    45,
      36,    29,    23,    18,    15
};

uint64_t cfs_nice_to_weight(int nice)
{
    if (nice < CFS_NICE_MIN) nice = CFS_NICE_MIN;
    if (nice > CFS_NICE_MAX) nice = CFS_NICE_MAX;
    return nice_to_weight[nice - CFS_NICE_MIN];
}

static rb_node_t sentinel;
static int rbtree_initialized = 0;

void rbtree_init(rb_tree_t* tree)
{
    if (!rbtree_initialized) {
        /* A red-black tree nil node is dereferenced by delete fixups
         * (w->left/right->color).  Keep it self-referential instead of
         * NULL-backed, otherwise removing some ready tasks can corrupt the
         * scheduler or fault under load. */
        sentinel.parent = &sentinel;
        sentinel.left = &sentinel;
        sentinel.right = &sentinel;
        sentinel.color = 0;
        rbtree_initialized = 1;
    }
    tree->nil = &sentinel;
    tree->root = tree->nil;
}

static void __rotate_left(rb_tree_t* tree, rb_node_t* x)
{
    rb_node_t* y = x->right;
    x->right = y->left;
    if (y->left != tree->nil) y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == tree->nil) tree->root = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void __rotate_right(rb_tree_t* tree, rb_node_t* y)
{
    rb_node_t* x = y->left;
    y->left = x->right;
    if (x->right != tree->nil) x->right->parent = y;
    x->parent = y->parent;
    if (y->parent == tree->nil) tree->root = x;
    else if (y == y->parent->right) y->parent->right = x;
    else y->parent->left = x;
    x->right = y;
    y->parent = x;
}

static void __insert_fixup(rb_tree_t* tree, rb_node_t* z)
{
    while (z->parent->color == 1) {
        if (z->parent == z->parent->parent->left) {
            rb_node_t* y = z->parent->parent->right;
            if (y->color == 1) {
                z->parent->color = 0;
                y->color = 0;
                z->parent->parent->color = 1;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    __rotate_left(tree, z);
                }
                z->parent->color = 0;
                z->parent->parent->color = 1;
                __rotate_right(tree, z->parent->parent);
            }
        } else {
            rb_node_t* y = z->parent->parent->left;
            if (y->color == 1) {
                z->parent->color = 0;
                y->color = 0;
                z->parent->parent->color = 1;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    __rotate_right(tree, z);
                }
                z->parent->color = 0;
                z->parent->parent->color = 1;
                __rotate_left(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = 0;
}

static uint64_t __node_key(rb_node_t* node)
{
    cfs_task_t* task = (cfs_task_t*)((uint8_t*)node - offsetof(cfs_task_t, rb_node));
    return task->vruntime;
}

void rbtree_insert(rb_tree_t* tree, rb_node_t* node, uint64_t key)
{
    (void)key;
    rb_node_t* y = tree->nil;
    rb_node_t* x = tree->root;

    node->left = node->right = tree->nil;
    node->color = 1;

    while (x != tree->nil) {
        y = x;
        if (__node_key(node) < __node_key(x))
            x = x->left;
        else
            x = x->right;
    }

    node->parent = y;
    if (y == tree->nil) {
        tree->root = node;
    } else if (__node_key(node) < __node_key(y)) {
        y->left = node;
    } else {
        y->right = node;
    }

    __insert_fixup(tree, node);
}

static void __transplant(rb_tree_t* tree, rb_node_t* u, rb_node_t* v)
{
    if (u->parent == tree->nil) tree->root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    v->parent = u->parent;
}

static void __delete_fixup(rb_tree_t* tree, rb_node_t* x)
{
    while (x != tree->root && x->color == 0) {
        if (x == x->parent->left) {
            rb_node_t* w = x->parent->right;
            if (w->color == 1) {
                w->color = 0;
                x->parent->color = 1;
                __rotate_left(tree, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == 0 && w->right->color == 0) {
                w->color = 1;
                x = x->parent;
            } else {
                if (w->right->color == 0) {
                    w->left->color = 0;
                    w->color = 1;
                    __rotate_right(tree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = 0;
                w->right->color = 0;
                __rotate_left(tree, x->parent);
                x = tree->root;
            }
        } else {
            rb_node_t* w = x->parent->left;
            if (w->color == 1) {
                w->color = 0;
                x->parent->color = 1;
                __rotate_right(tree, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == 0 && w->left->color == 0) {
                w->color = 1;
                x = x->parent;
            } else {
                if (w->left->color == 0) {
                    w->right->color = 0;
                    w->color = 1;
                    __rotate_left(tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = 0;
                w->left->color = 0;
                __rotate_right(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = 0;
}

static rb_node_t* __tree_min(rb_tree_t* tree, rb_node_t* x)
{
    while (x->left != tree->nil) x = x->left;
    return x;
}

void rbtree_delete(rb_tree_t* tree, rb_node_t* z)
{
    rb_node_t* y = z;
    rb_node_t* x;
    int y_original_color = y->color;

    if (z->left == tree->nil) {
        x = z->right;
        __transplant(tree, z, z->right);
    } else if (z->right == tree->nil) {
        x = z->left;
        __transplant(tree, z, z->left);
    } else {
        y = __tree_min(tree, z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            __transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        __transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    if (y_original_color == 0) {
        __delete_fixup(tree, x);
    }
}

rb_node_t* rbtree_min(rb_tree_t* tree)
{
    rb_node_t* x = tree->root;
    if (x == tree->nil) return NULL;
    while (x->left != tree->nil) x = x->left;
    return x;
}

static cfs_rq_t global_rq;
static uint64_t cfs_jiffies = 0;

void cfs_init(void)
{
    rbtree_init(&global_rq.tasks_timeline);
    spin_init(&global_rq.lock);
    global_rq.min_vruntime = 0;
    global_rq.exec_clock = 0;
    global_rq.nr_running = 0;
    global_rq.nr_tasks = 0;
    global_rq.curr = NULL;
    global_rq.idle = NULL;
}

void cfs_rq_init(cfs_rq_t* rq)
{
    rbtree_init(&rq->tasks_timeline);
    spin_init(&rq->lock);
    rq->min_vruntime = 0;
    rq->exec_clock = 0;
    rq->nr_running = 0;
    rq->nr_tasks = 0;
    rq->curr = NULL;
    rq->idle = NULL;
}

void cfs_task_init(cfs_task_t* task, int nice)
{
    memset(task, 0, sizeof(cfs_task_t));
    task->nice = nice;
    task->load_weight = cfs_nice_to_weight(nice);
    task->vruntime = global_rq.min_vruntime;
    task->on_rq = 0;
}

void cfs_enqueue_task(cfs_rq_t* rq, cfs_task_t* task)
{
    spin_lock(&rq->lock);

    if (task->on_rq) {
        spin_unlock(&rq->lock);
        return;
    }

    if (task->vruntime < rq->min_vruntime - CFS_TARGET_LATENCY * 4) {
        task->vruntime = rq->min_vruntime - CFS_TARGET_LATENCY * 4;
    }

    rbtree_insert(&rq->tasks_timeline, &task->rb_node, task->vruntime);
    task->on_rq = 1;
    rq->nr_running++;
    rq->nr_tasks++;

    spin_unlock(&rq->lock);
}

void cfs_dequeue_task(cfs_rq_t* rq, cfs_task_t* task)
{
    spin_lock(&rq->lock);

    if (!task->on_rq) {
        spin_unlock(&rq->lock);
        return;
    }

    rbtree_delete(&rq->tasks_timeline, &task->rb_node);
    task->rb_node.parent = NULL;
    task->rb_node.left = NULL;
    task->rb_node.right = NULL;
    task->on_rq = 0;
    rq->nr_running--;
    if (rq->nr_tasks > 0) {
        rq->nr_tasks--;
    }

    spin_unlock(&rq->lock);
}

cfs_task_t* cfs_pick_next_task(cfs_rq_t* rq)
{
    spin_lock(&rq->lock);

    rb_node_t* node = rbtree_min(&rq->tasks_timeline);
    if (!node) {
        spin_unlock(&rq->lock);
        return rq->idle;
    }

    cfs_task_t* task = (cfs_task_t*)((uint8_t*)node - offsetof(cfs_task_t, rb_node));
    spin_unlock(&rq->lock);
    return task;
}

void cfs_account_exec(cfs_rq_t* rq, cfs_task_t* task, uint64_t delta_ns)
{
    if (!rq || !task) return;

    uint64_t delta_exec = delta_ns;
    if (task->load_weight != CFS_NICE_0_LOAD) {
        delta_exec = delta_ns * CFS_NICE_0_LOAD / task->load_weight;
    }

    task->vruntime += delta_exec;
    task->sum_exec_runtime += delta_ns;
    rq->exec_clock += delta_ns;
}

void cfs_update_min_vruntime(cfs_rq_t* rq)
{
    rb_node_t* node = rbtree_min(&rq->tasks_timeline);
    if (node) {
        cfs_task_t* task = (cfs_task_t*)((uint8_t*)node - offsetof(cfs_task_t, rb_node));
        if (task->vruntime > rq->min_vruntime) {
            rq->min_vruntime = task->vruntime;
        }
    }
    if (rq->curr && rq->curr->vruntime > rq->min_vruntime) {
        rq->min_vruntime = rq->curr->vruntime;
    }
}

void cfs_scheduler_tick(cfs_rq_t* rq)
{
    cfs_jiffies++;

    if (!rq->curr) return;

    cfs_task_t* curr = rq->curr;

    cfs_account_exec(rq, curr, CFS_TICK_NSEC);

    uint64_t slice = CFS_TARGET_LATENCY;
    if (rq->nr_running > 1) {
        slice = CFS_TARGET_LATENCY / rq->nr_running;
    }
    if (slice < CFS_MIN_GRANULARITY) slice = CFS_MIN_GRANULARITY;

    cfs_task_t* next = cfs_pick_next_task(rq);
    if (next && next != curr) {
        if ((int64_t)(curr->vruntime - next->vruntime) > (int64_t)slice) {

        }
    }

    cfs_update_min_vruntime(rq);
}

cfs_rq_t* cfs_get_rq(void)
{
    return &global_rq;
}