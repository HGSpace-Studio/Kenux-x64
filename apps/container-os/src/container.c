#include "container.h"
#include <string.h>

static uint32_t s_next_id = 1;

static void generate_id(char *buf, size_t len)
{
    const char *hex = "0123456789abcdef";
    uint32_t id = s_next_id++;
    int pos = (int)len - 1;
    buf[--pos] = '\0';
    if (id == 0) {
        buf[--pos] = '0';
    } else {
        while (id > 0 && pos > 4) {
            buf[--pos] = hex[id & 0xf];
            id >>= 4;
        }
    }
    buf[--pos] = 'n';
    buf[--pos] = 't';
    buf[--pos] = 'c';
    memmove(buf, buf + pos, len - (size_t)pos);
}

static void safe_strncpy(char *dst, const char *src, size_t n)
{
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

int container_create(container_t *c, const char *name, const char *image,
                     const char *command)
{
    if (!c || !name || !image) return -1;

    memset(c, 0, sizeof(*c));
    generate_id(c->id, sizeof(c->id));
    safe_strncpy(c->name, name, sizeof(c->name));
    safe_strncpy(c->image, image, sizeof(c->image));
    safe_strncpy(c->command, command ? command : "", sizeof(c->command));
    c->status = CONTAINER_CREATED;
    c->resources.cpu_limit = 0;
    c->resources.memory_limit = 0;
    c->pid = 0;

    container_append_log(c, "Container created");
    return 0;
}

int container_start(container_t *c)
{
    if (!c) return -1;
    if (c->status == CONTAINER_RUNNING) return 0;

    c->status = CONTAINER_RUNNING;
    container_append_log(c, "Container started");
    return 0;
}

int container_stop(container_t *c)
{
    if (!c) return -1;
    if (c->status != CONTAINER_RUNNING) return -1;

    c->status = CONTAINER_STOPPED;
    container_append_log(c, "Container stopped");
    return 0;
}

int container_destroy(container_t *c)
{
    if (!c) return -1;
    if (c->status == CONTAINER_RUNNING) {
        container_stop(c);
    }
    container_append_log(c, "Container destroyed");
    c->status = CONTAINER_ERROR;
    return 0;
}

void container_append_log(container_t *c, const char *msg)
{
    if (!c || !msg) return;
    size_t msg_len = strlen(msg);
    if (c->log_len + (int)msg_len + 1 >= CONTAINER_LOG_SIZE) {
        memmove(c->logs, c->logs + (CONTAINER_LOG_SIZE / 2), CONTAINER_LOG_SIZE / 2);
        c->log_len = CONTAINER_LOG_SIZE / 2;
    }
    memcpy(c->logs + c->log_len, msg, msg_len);
    c->log_len += (int)msg_len;
    c->logs[c->log_len++] = '\n';
    c->logs[c->log_len] = '\0';
}

const char *container_status_str(container_status_t status)
{
    switch (status) {
        case CONTAINER_CREATED: return "Created";
        case CONTAINER_RUNNING: return "Running";
        case CONTAINER_STOPPED: return "Stopped";
        case CONTAINER_PAUSED:  return "Paused";
        case CONTAINER_ERROR:   return "Error";
        default:                return "Unknown";
    }
}
