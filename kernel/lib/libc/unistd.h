#ifndef LIBC_UNISTD_H
#define LIBC_UNISTD_H

#include <arch/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int access(const char* pathname, int mode);
unsigned int sleep(unsigned int seconds);
unsigned int usleep(useconds_t usec);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int close(int fd);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int execve(const char* path, char* const argv[], char* const envp[]);
int fork(void);
int vfork(void);
int kill(pid_t pid, int sig);
int link(const char* oldpath, const char* newpath);
int unlink(const char* pathname);
long read(int fd, void* buf, size_t count);
long write(int fd, const void* buf, size_t count);
long lseek(int fd, long offset, int whence);
int open(const char* pathname, int flags, ...);
int pipe(int pipefd[2]);
int rename(const char* oldpath, const char* newpath);
int symlink(const char* target, const char* linkpath);
ssize_t readlink(const char* path, char* buf, size_t bufsiz);
int truncate(const char* path, off_t length);
int ftruncate(int fd, off_t length);

#endif
