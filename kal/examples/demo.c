/* SPDX-License-Identifier: MIT
 * demo.c - KAL 端到端演示
 *
 * 演示场景:
 *   1. 启动默认 backend(linuxlike),外壳调 open/read/write/mmap/fork/ipc
 *   2. 多内核并存路由:把 IPC 域绑定到 microkernel
 *   3. 热切换:linuxlike -> custom,切换后再调一遍 SSI,外壳代码不变
 *   4. 错误码映射:custom 不支持网络栈,kal_socket 返回 KAL_ENOTSUP
 *   5. Fallback:切到 micro 后,kal_fork 走 fallback 路径(因 mk 无 fork)
 *   6. 版本协商与状态查询
 */
#include "kal.h"

#include <stdio.h>
#include <string.h>

static void line(const char *t) {
    printf("\n================ %s ================\n", t);
}

static void dump_active(void) {
    const kal_backend_state_t *s = kal_governance_active_state();
    if (!s) { printf("  (no active backend)\n"); return; }
    printf("  active backend : %s\n", s->ops->name);
    printf("  healthy        : %d\n", s->healthy);
    printf("  fail_count     : %u\n", s->fail_count);
    printf("  caps.io_uring  : %d\n", kal_cap_has(&s->caps, KAL_CAP_IO_URING));
    printf("  caps.net_stack : %d\n", kal_cap_has(&s->caps, KAL_CAP_NET_STACK));
    printf("  caps.fork      : %d\n", kal_cap_has(&s->caps, KAL_CAP_FORK));
}

static void exercise_ssi(void) {
    /* 文件系统 */
    kal_fd_t fd = kal_open("/etc/demo.conf", 0, 0644);
    printf("  kal_open -> fd=%d\n", fd);
    if (fd >= 0) {
        char buf[64] = {0};
        long r = kal_read(fd, buf, sizeof(buf));
        printf("  kal_read -> %ld\n", r);
        long w = kal_write(fd, "hello", 5);
        printf("  kal_write -> %ld\n", w);
        int c = kal_close(fd);
        printf("  kal_close -> %d (%s)\n", c, kal_strerror(c));
    }
    /* 内存 */
    struct kal_mmap_args ma = {0};
    ma.length = 4096;
    ma.prot   = KAL_PROT_READ | KAL_PROT_WRITE;
    ma.flags  = KAL_MAP_ANON | KAL_MAP_PRIVATE;
    void *p = kal_mmap(&ma);
    printf("  kal_mmap -> %p\n", p);
    if (p) {
        int u = kal_munmap(p, ma.length);
        printf("  kal_munmap -> %d\n", u);
    }
    /* 进程 */
    kal_pid_t child = kal_fork();
    printf("  kal_fork -> %d (%s)\n", (int)child,
           child >= 0 ? "ok" : kal_strerror((kal_err_t)child));
    /* IPC */
    kal_chan_t ch = { .id = 0xdeadbeef, .backend_tag = 1 };
    kal_err_t e = kal_ipc_send(&ch, "ping", 4);
    printf("  kal_ipc_send -> %d (%s)\n", e, kal_strerror(e));
    /* 网络(默认 linuxlike 支持) */
    int sock = kal_socket(2, 1, 0);
    printf("  kal_socket -> %d\n", sock);
}

int main(void) {
    line("1. boot KAL with default backend=linuxlike");
    struct kal_cfg cfg = {
        .backend_name        = "linuxlike",
        .heartbeat_ms        = 1000,
        .failover_threshold  = 3,
        .drain_timeout_ms    = 500,
        .allow_degrade       = 1,
    };
    kal_err_t e = kal_init(&cfg);
    if (e != KAL_OK) {
        fprintf(stderr, "kal_init failed: %s\n", kal_strerror(e));
        return 1;
    }
    dump_active();

    line("2. exercise SSI on linuxlike");
    exercise_ssi();

    line("3. bind IPC domain to microkernel (multi-kernel routing)");
    kal_dispatcher_bind_domain(KAL_DOMAIN_IPC, "micro");
    /* 触发一次 IPC,应看到 micro 路径 */
    kal_chan_t ch = { .id = 0xcafebabe, .backend_tag = 2 };
    kal_ipc_send(&ch, "route-test", 10);

    line("4. version negotiation");
    kal_version_info_t v = kal_governance_version();
    printf("  ssi=%u.%u backend=%u.%u compatible=%d\n",
           v.ssi_major, v.ssi_minor, v.backend_major, v.backend_minor, v.compatible);

    line("5. hot-swap linuxlike -> custom (shell code unchanged)");
    e = kal_governance_hot_swap("custom");
    printf("  hot_swap result: %s\n", kal_strerror(e));
    dump_active();

    line("6. exercise SSI again on custom");
    exercise_ssi();

    line("7. error mapping: custom has no net stack");
    int sock = kal_socket(2, 1, 0);
    printf("  kal_socket -> %d (%s)\n", sock,
           sock < 0 ? kal_strerror((kal_err_t)sock) : "ok");

    line("8. health tick + state");
    int h = kal_governance_health_tick();
    printf("  health_tick -> %d, state=%d\n", h, kal_governance_state());

    line("9. hot-swap custom -> micro (fork goes through clone+exec simulation)");
    e = kal_governance_hot_swap("micro");
    printf("  hot_swap result: %s\n", kal_strerror(e));
    dump_active();

    kal_fd_t fd = kal_open("/tmp/x", 0, 0);
    printf("  kal_open on micro -> fd=%d\n", fd);
    kal_pid_t child = kal_fork();
    printf("  kal_fork on micro -> %d (%s)\n", (int)child,
           child >= 0 ? "ok" : kal_strerror((kal_err_t)child));

    line("10. shutdown");
    kal_exit();
    printf("done.\n");
    return 0;
}
