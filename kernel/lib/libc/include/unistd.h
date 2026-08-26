#ifndef UNISTD_H
#define UNISTD_H

#include <arch/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _SC_PAGESIZE 0
#define _SC_OPEN_MAX 1

pid_t getpid(void);
pid_t getppid(void);
int fork(void);
int execve(const char* filename, char* const argv[], char* const envp[]);
int execv(const char* filename, char* const argv[]);
int execvp(const char* filename, char* const argv[]);
int wait(int* status);
int waitpid(pid_t pid, int* status, int options);
int pipe(int pipefd[2]);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int close(int fd);
int read(int fd, void* buf, size_t count);
int write(int fd, const void* buf, size_t count);
int open(const char* pathname, int flags, ...);
int creat(const char* pathname, mode_t mode);
int lseek(int fd, off_t offset, int whence);
unsigned int sleep(unsigned int seconds);
unsigned int usleep(useconds_t usec);
int unlink(const char* pathname);
int rename(const char* oldpath, const char* newpath);
int mkdir(const char* pathname, mode_t mode);
int rmdir(const char* pathname);
int chmod(const char* pathname, mode_t mode);
int chown(const char* pathname, uid_t owner, gid_t group);
long sysconf(int name);
int getuid(void);
int getgid(void);
int setuid(uid_t uid);
int setgid(gid_t gid);
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int system(const char* command);
void _exit(int status);
void exit(int status);

#endif
