#pragma once

#include "sys/types.h"
#include "sys/stat.h"

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000

#define IPC_RMID 0
#define IPC_SET  1
#define IPC_STAT 2
#define IPC_INFO 3

#define IPC_PRIVATE ((key_t)0)

typedef int key_t;

struct ipc_perm {
    key_t    key;
    uid_t    uid;
    gid_t    gid;
    uid_t    cuid;
    gid_t    cgid;
    mode_t   mode;
    unsigned short seq;
};

struct msqid_ds {
    struct ipc_perm msg_perm;
    time_t          msg_stime;
    time_t          msg_rtime;
    time_t          msg_ctime;
    unsigned long   msg_cbytes;
    unsigned long   msg_qnum;
    unsigned long   msg_qbytes;
    pid_t           msg_lspid;
    pid_t           msg_lrpid;
};

struct msginfo {
    int msgpool;
    int msgmap;
    int msgmax;
    int msgmnb;
    int msgmni;
    int msgssz;
    int msgtql;
    unsigned short msgseg;
};

struct semid_ds {
    struct ipc_perm sem_perm;
    time_t          sem_otime;
    time_t          sem_ctime;
    unsigned short  sem_nsems;
};

struct sembuf {
    unsigned short sem_num;
    short          sem_op;
    short          sem_flg;
};

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t          shm_segsz;
    time_t          shm_atime;
    time_t          shm_dtime;
    time_t          shm_ctime;
    pid_t           shm_cpid;
    pid_t           shm_lpid;
    unsigned long   shm_nattch;
};

#ifdef __cplusplus
extern "C" {
#endif

key_t ftok(const char *pathname, int proj_id);

int   msgget(key_t key, int msgflg);
int   msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
int   msgctl(int msqid, int cmd, struct msqid_ds *buf);

int   semget(key_t key, int nsems, int semflg);
int   semop(int semid, struct sembuf *sops, size_t nsops);
int   semctl(int semid, int semnum, int cmd, ...);

int   shmget(key_t key, size_t size, int shmflg);
void *shmat(int shmid, const void *shmaddr, int shmflg);
int   shmdt(const void *shmaddr);
int   shmctl(int shmid, int cmd, struct shmid_ds *buf);

#ifdef __cplusplus
}
#endif