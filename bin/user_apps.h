#ifndef USER_APPS_H
#define USER_APPS_H

void process_list_all(void);
void process_kill(int pid);
void text_editor_run(void);
void desktop_run(void);
void syscall_halt(void);
void getcwd(char* buf, size_t size);

#endif
