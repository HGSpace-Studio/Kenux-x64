/* SPDX-License-Identifier: MIT
 * kal_ssi.h - SSI(System Service Interface)对外壳暴露的 C ABI 入口
 *
 * 这是整个设计的"稳定不变量":无论内核怎么换,这些函数签名不变。
 * 外壳层(GUI/系统服务/用户运行时/应用)只允许依赖本头文件。
 *
 * 对应架构文档"七、后续可选工作"第 1 项:
 *   "用具体 IDL 写出第一批核心 SSI 接口(open/read/write/mmap/fork/exec)。"
 */
#ifndef KAL_SSI_H
#define KAL_SSI_H

#include "kal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 文件系统 ---- */
kal_fd_t   kal_open(const char *path, int flags, int mode);
int        kal_close(kal_fd_t fd);
long       kal_read(kal_fd_t fd, void *buf, kal_size_t n);
long       kal_write(kal_fd_t fd, const void *buf, kal_size_t n);
int        kal_stat(const char *path, struct kal_stat *out);

/* ---- 内存 ---- */
void      *kal_mmap(struct kal_mmap_args *args);
int        kal_munmap(void *addr, kal_size_t length);

/* ---- 进程 / 线程 ---- */
kal_pid_t  kal_fork(void);
int        kal_exec(const char *path, char *const argv[], char *const envp[]);
int        kal_wait(kal_pid_t pid, int *out_status, int options);
int        kal_sched_setaffinity(kal_pid_t pid, size_t cpusetsize,
                                 const unsigned char *mask);

/* ---- 设备 I/O ---- */
int        kal_ioctl(kal_fd_t fd, unsigned long request, void *arg);

/* ---- 网络(可选能力;不支持则返回 KAL_ENOTSUP) ---- */
int        kal_socket(int domain, int type, int protocol);
int        kal_bind(int fd, const void *addr, size_t addrlen);
int        kal_connect(int fd, const void *addr, size_t addrlen);
long       kal_sendmsg(int fd, const void *msg, int flags);
long       kal_recvmsg(int fd, void *msg, int flags);

/* ---- IPC 通道 ---- */
kal_err_t  kal_ipc_send(kal_chan_t *ch, const void *data, kal_size_t n);
kal_err_t  kal_ipc_recv(kal_chan_t *ch, void *buf, kal_size_t n, int timeout_ms);

/* ---- 时间(由 KAL 自身提供,不强求内核) ---- */
uint64_t   kal_clock_gettime_ns(void);

/* ---- 错误转字符串(供外壳日志) ---- */
const char *kal_strerror(kal_err_t e);

#ifdef __cplusplus
}
#endif
#endif /* KAL_SSI_H */
