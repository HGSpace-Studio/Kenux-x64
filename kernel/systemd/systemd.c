#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>
#include <cgroup.h>

systemd_t systemd;

unit_t* find_unit(const char* name)
{
    for (int i = 0; i < systemd.unit_count; i++) {
        if (strcmp(systemd.units[i]->name, name) == 0) {
            return systemd.units[i];
        }
    }
    return NULL;
}

static service_t* find_service(const char* name)
{
    for (int i = 0; i < systemd.service_count; i++) {
        if (strcmp(systemd.services[i].base.name, name) == 0) {
            return &systemd.services[i];
        }
    }
    return NULL;
}

static target_t* find_target(const char* name)
{
    for (int i = 0; i < systemd.target_count; i++) {
        if (strcmp(systemd.targets[i].base.name, name) == 0) {
            return &systemd.targets[i];
        }
    }
    return NULL;
}

static systemd_socket_t* find_socket(const char* name)
{
    for (int i = 0; i < systemd.socket_count; i++) {
        if (strcmp(systemd.sockets[i].base.name, name) == 0) {
            return &systemd.sockets[i];
        }
    }
    return NULL;
}

static timer_t* find_timer(const char* name)
{
    for (int i = 0; i < systemd.timer_count; i++) {
        if (strcmp(systemd.timers[i].base.name, name) == 0) {
            return &systemd.timers[i];
        }
    }
    return NULL;
}

static mount_t* find_mount(const char* name)
{
    for (int i = 0; i < systemd.mount_count; i++) {
        if (strcmp(systemd.mounts[i].base.name, name) == 0) {
            return &systemd.mounts[i];
        }
    }
    return NULL;
}

static slice_t* find_slice(const char* name)
{
    for (int i = 0; i < systemd.slice_count; i++) {
        if (strcmp(systemd.slices[i].base.name, name) == 0) {
            return &systemd.slices[i];
        }
    }
    return NULL;
}

static device_t* find_device(const char* name)
{
    for (int i = 0; i < systemd.device_count; i++) {
        if (strcmp(systemd.devices[i].base.name, name) == 0) {
            return &systemd.devices[i];
        }
    }
    return NULL;
}

static automount_t* find_automount(const char* name)
{
    for (int i = 0; i < systemd.automount_count; i++) {
        if (strcmp(systemd.automounts[i].base.name, name) == 0) {
            return &systemd.automounts[i];
        }
    }
    return NULL;
}

static systemd_path_t* find_path(const char* name)
{
    for (int i = 0; i < systemd.path_count; i++) {
        if (strcmp(systemd.paths[i].base.name, name) == 0) {
            return &systemd.paths[i];
        }
    }
    return NULL;
}

static swap_t* find_swap(const char* name)
{
    for (int i = 0; i < systemd.swap_count; i++) {
        if (strcmp(systemd.swaps[i].base.name, name) == 0) {
            return &systemd.swaps[i];
        }
    }
    return NULL;
}

static void generate_boot_id(void)
{
    for (int i = 0; i < 16; i++) {
        systemd.boot_id[i] = (uint64_t)(rand() & 0xFF) | 
                            ((uint64_t)(rand() & 0xFF) << 8) |
                            ((uint64_t)(rand() & 0xFF) << 16) |
                            ((uint64_t)(rand() & 0xFF) << 24) |
                            ((uint64_t)(rand() & 0xFF) << 32) |
                            ((uint64_t)(rand() & 0xFF) << 40) |
                            ((uint64_t)(rand() & 0xFF) << 48) |
                            ((uint64_t)(rand() & 0xFF) << 56);
    }
}

static void init_cgroup_slices(void)
{
    vfs_mkdir("/sys/fs/cgroup", 0755);
    vfs_mkdir("/sys/fs/cgroup/systemd", 0755);
    
    cgroup_init();
    struct cgroup* root = cgroup_find_by_name("/");
    if (root) {
        cgroup_create(root, "system.slice");
        cgroup_create(root, "user.slice");
        cgroup_create(root, "machine.slice");
        cgroup_create(root, "init.scope");
        
        struct cgroup* system_slice = cgroup_find_by_name("/system.slice");
        if (system_slice) {
            cgroup_create(system_slice, "systemd-journald");
            cgroup_create(system_slice, "systemd-udevd");
            cgroup_create(system_slice, "systemd-networkd");
        }
    }
}

static void load_builtin_units(void)
{
    service_t* svc;
    target_t* tgt;
    systemd_socket_t* sock;
    timer_t* tmr;
    mount_t* mnt;
    slice_t* slc;
    
    svc = &systemd.services[systemd.service_count++];
    memset(svc, 0, sizeof(service_t));
    INIT_LIST_HEAD(&svc->base.unit_list);
    INIT_LIST_HEAD(&svc->sockets);
    INIT_LIST_HEAD(&svc->cgroup_list);
    INIT_LIST_HEAD(&svc->service_list);
    strcpy(svc->base.name, "container-os");
    strcpy(svc->base.description, "Container OS Service");
    strcpy(svc->working_dir, "/");
    svc->type = SERVICE_TYPE_SIMPLE;
    svc->restart = RESTART_ALWAYS;
    svc->restart_sec = 5;
    svc->base.enabled = true;
    svc->base.type = UNIT_TYPE_SERVICE;
    svc->base.state = UNIT_STATE_DEAD;
    svc->base.load_time = time_get_timestamp();
    list_add_tail(&svc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &svc->base;
    
    svc = &systemd.services[systemd.service_count++];
    memset(svc, 0, sizeof(service_t));
    INIT_LIST_HEAD(&svc->base.unit_list);
    INIT_LIST_HEAD(&svc->sockets);
    INIT_LIST_HEAD(&svc->cgroup_list);
    INIT_LIST_HEAD(&svc->service_list);
    strcpy(svc->base.name, "shell");
    strcpy(svc->base.description, "Shell Service");
    strcpy(svc->working_dir, "/");
    svc->type = SERVICE_TYPE_SIMPLE;
    svc->restart = RESTART_ALWAYS;
    svc->restart_sec = 2;
    svc->base.enabled = true;
    svc->base.type = UNIT_TYPE_SERVICE;
    svc->base.state = UNIT_STATE_DEAD;
    svc->base.load_time = time_get_timestamp();
    list_add_tail(&svc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &svc->base;
    
    svc = &systemd.services[systemd.service_count++];
    memset(svc, 0, sizeof(service_t));
    INIT_LIST_HEAD(&svc->base.unit_list);
    INIT_LIST_HEAD(&svc->sockets);
    INIT_LIST_HEAD(&svc->cgroup_list);
    INIT_LIST_HEAD(&svc->service_list);
    strcpy(svc->base.name, "syslog");
    strcpy(svc->base.description, "System Logging Service");
    svc->base.type = UNIT_TYPE_SERVICE;
    svc->base.state = UNIT_STATE_DEAD;
    svc->base.load_time = time_get_timestamp();
    svc->base.enabled = true;
    list_add_tail(&svc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &svc->base;
    
    svc = &systemd.services[systemd.service_count++];
    memset(svc, 0, sizeof(service_t));
    INIT_LIST_HEAD(&svc->base.unit_list);
    INIT_LIST_HEAD(&svc->sockets);
    INIT_LIST_HEAD(&svc->cgroup_list);
    INIT_LIST_HEAD(&svc->service_list);
    strcpy(svc->base.name, "network");
    strcpy(svc->base.description, "Network Service");
    svc->base.type = UNIT_TYPE_SERVICE;
    svc->base.state = UNIT_STATE_DEAD;
    svc->base.load_time = time_get_timestamp();
    svc->base.enabled = true;
    list_add_tail(&svc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &svc->base;
    
    svc = &systemd.services[systemd.service_count++];
    memset(svc, 0, sizeof(service_t));
    INIT_LIST_HEAD(&svc->base.unit_list);
    INIT_LIST_HEAD(&svc->sockets);
    INIT_LIST_HEAD(&svc->cgroup_list);
    INIT_LIST_HEAD(&svc->service_list);
    strcpy(svc->base.name, "udev");
    strcpy(svc->base.description, "Device Manager");
    svc->base.type = UNIT_TYPE_SERVICE;
    svc->base.state = UNIT_STATE_DEAD;
    svc->base.load_time = time_get_timestamp();
    svc->base.enabled = true;
    list_add_tail(&svc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &svc->base;
    
    svc = &systemd.services[systemd.service_count++];
    memset(svc, 0, sizeof(service_t));
    INIT_LIST_HEAD(&svc->base.unit_list);
    INIT_LIST_HEAD(&svc->sockets);
    INIT_LIST_HEAD(&svc->cgroup_list);
    INIT_LIST_HEAD(&svc->service_list);
    strcpy(svc->base.name, "journald");
    strcpy(svc->base.description, "Journal Service");
    svc->type = SERVICE_TYPE_SIMPLE;
    svc->base.type = UNIT_TYPE_SERVICE;
    svc->base.state = UNIT_STATE_DEAD;
    svc->base.load_time = time_get_timestamp();
    svc->base.enabled = true;
    list_add_tail(&svc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &svc->base;
    
    tgt = &systemd.targets[systemd.target_count++];
    memset(tgt, 0, sizeof(target_t));
    INIT_LIST_HEAD(&tgt->base.unit_list);
    INIT_LIST_HEAD(&tgt->services);
    INIT_LIST_HEAD(&tgt->targets);
    INIT_LIST_HEAD(&tgt->sockets);
    strcpy(tgt->base.name, "sysinit");
    strcpy(tgt->base.description, "System Initialization");
    tgt->base.type = UNIT_TYPE_TARGET;
    tgt->base.state = UNIT_STATE_DEAD;
    tgt->base.load_time = time_get_timestamp();
    list_add_tail(&tgt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tgt->base;
    
    tgt = &systemd.targets[systemd.target_count++];
    memset(tgt, 0, sizeof(target_t));
    INIT_LIST_HEAD(&tgt->base.unit_list);
    INIT_LIST_HEAD(&tgt->services);
    INIT_LIST_HEAD(&tgt->targets);
    INIT_LIST_HEAD(&tgt->sockets);
    strcpy(tgt->base.name, "basic");
    strcpy(tgt->base.description, "Basic System");
    tgt->base.type = UNIT_TYPE_TARGET;
    tgt->base.state = UNIT_STATE_DEAD;
    tgt->base.load_time = time_get_timestamp();
    list_add_tail(&tgt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tgt->base;
    
    tgt = &systemd.targets[systemd.target_count++];
    memset(tgt, 0, sizeof(target_t));
    INIT_LIST_HEAD(&tgt->base.unit_list);
    INIT_LIST_HEAD(&tgt->services);
    INIT_LIST_HEAD(&tgt->targets);
    INIT_LIST_HEAD(&tgt->sockets);
    strcpy(tgt->base.name, "multi-user");
    strcpy(tgt->base.description, "Multi-User System");
    tgt->base.type = UNIT_TYPE_TARGET;
    tgt->base.state = UNIT_STATE_DEAD;
    tgt->base.load_time = time_get_timestamp();
    list_add_tail(&tgt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tgt->base;
    
    tgt = &systemd.targets[systemd.target_count++];
    memset(tgt, 0, sizeof(target_t));
    INIT_LIST_HEAD(&tgt->base.unit_list);
    INIT_LIST_HEAD(&tgt->services);
    INIT_LIST_HEAD(&tgt->targets);
    INIT_LIST_HEAD(&tgt->sockets);
    strcpy(tgt->base.name, "graphical");
    strcpy(tgt->base.description, "Graphical Interface");
    tgt->base.type = UNIT_TYPE_TARGET;
    tgt->base.state = UNIT_STATE_DEAD;
    tgt->base.load_time = time_get_timestamp();
    list_add_tail(&tgt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tgt->base;
    
    tgt = &systemd.targets[systemd.target_count++];
    memset(tgt, 0, sizeof(target_t));
    INIT_LIST_HEAD(&tgt->base.unit_list);
    INIT_LIST_HEAD(&tgt->services);
    INIT_LIST_HEAD(&tgt->targets);
    INIT_LIST_HEAD(&tgt->sockets);
    strcpy(tgt->base.name, "rescue");
    strcpy(tgt->base.description, "Rescue Mode");
    tgt->base.type = UNIT_TYPE_TARGET;
    tgt->base.state = UNIT_STATE_DEAD;
    tgt->base.load_time = time_get_timestamp();
    list_add_tail(&tgt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tgt->base;
    
    tgt = &systemd.targets[systemd.target_count++];
    memset(tgt, 0, sizeof(target_t));
    INIT_LIST_HEAD(&tgt->base.unit_list);
    INIT_LIST_HEAD(&tgt->services);
    INIT_LIST_HEAD(&tgt->targets);
    INIT_LIST_HEAD(&tgt->sockets);
    strcpy(tgt->base.name, "emergency");
    strcpy(tgt->base.description, "Emergency Mode");
    tgt->base.type = UNIT_TYPE_TARGET;
    tgt->base.state = UNIT_STATE_DEAD;
    tgt->base.load_time = time_get_timestamp();
    list_add_tail(&tgt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tgt->base;
    
    sock = &systemd.sockets[systemd.socket_count++];
    memset(sock, 0, sizeof(systemd_socket_t));
    INIT_LIST_HEAD(&sock->base.unit_list);
    INIT_LIST_HEAD(&sock->service_link);
    strcpy(sock->base.name, "syslog");
    strcpy(sock->base.description, "Syslog Socket");
    strcpy(sock->socket_path, "/run/systemd/syslog.socket");
    sock->type = SOCKET_TYPE_DGRAM;
    sock->protocol = SOCKET_PROTOCOL_UNIX;
    sock->base.type = UNIT_TYPE_SOCKET;
    sock->base.state = UNIT_STATE_DEAD;
    sock->base.load_time = time_get_timestamp();
    sock->base.enabled = true;
    list_add_tail(&sock->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &sock->base;
    
    sock = &systemd.sockets[systemd.socket_count++];
    memset(sock, 0, sizeof(systemd_socket_t));
    INIT_LIST_HEAD(&sock->base.unit_list);
    INIT_LIST_HEAD(&sock->service_link);
    strcpy(sock->base.name, "journald");
    strcpy(sock->base.description, "Journald Socket");
    strcpy(sock->socket_path, "/run/systemd/journald.socket");
    sock->type = SOCKET_TYPE_DGRAM;
    sock->protocol = SOCKET_PROTOCOL_UNIX;
    sock->base.type = UNIT_TYPE_SOCKET;
    sock->base.state = UNIT_STATE_DEAD;
    sock->base.load_time = time_get_timestamp();
    sock->base.enabled = true;
    list_add_tail(&sock->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &sock->base;
    
    tmr = &systemd.timers[systemd.timer_count++];
    memset(tmr, 0, sizeof(timer_t));
    INIT_LIST_HEAD(&tmr->base.unit_list);
    strcpy(tmr->base.name, "journald");
    strcpy(tmr->base.description, "Journald Timer");
    strcpy(tmr->unit_name, "journald");
    tmr->base.type = UNIT_TYPE_TIMER;
    tmr->base.state = UNIT_STATE_DEAD;
    tmr->clock = TIMER_BOOTTIME;
    tmr->base.load_time = time_get_timestamp();
    list_add_tail(&tmr->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &tmr->base;
    
    mnt = &systemd.mounts[systemd.mount_count++];
    memset(mnt, 0, sizeof(mount_t));
    INIT_LIST_HEAD(&mnt->base.unit_list);
    strcpy(mnt->base.name, "sysfs");
    strcpy(mnt->base.description, "Sysfs Mount");
    strcpy(mnt->what, "sysfs");
    strcpy(mnt->where, "/sys");
    strcpy(mnt->type, "sysfs");
    mnt->base.type = UNIT_TYPE_MOUNT;
    mnt->base.state = UNIT_STATE_DEAD;
    mnt->base.load_time = time_get_timestamp();
    list_add_tail(&mnt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &mnt->base;
    
    mnt = &systemd.mounts[systemd.mount_count++];
    memset(mnt, 0, sizeof(mount_t));
    INIT_LIST_HEAD(&mnt->base.unit_list);
    strcpy(mnt->base.name, "proc");
    strcpy(mnt->base.description, "Proc Mount");
    strcpy(mnt->what, "proc");
    strcpy(mnt->where, "/proc");
    strcpy(mnt->type, "proc");
    mnt->base.type = UNIT_TYPE_MOUNT;
    mnt->base.state = UNIT_STATE_DEAD;
    mnt->base.load_time = time_get_timestamp();
    list_add_tail(&mnt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &mnt->base;
    
    mnt = &systemd.mounts[systemd.mount_count++];
    memset(mnt, 0, sizeof(mount_t));
    INIT_LIST_HEAD(&mnt->base.unit_list);
    strcpy(mnt->base.name, "tmpfs");
    strcpy(mnt->base.description, "Tmpfs Mount");
    strcpy(mnt->what, "tmpfs");
    strcpy(mnt->where, "/tmp");
    strcpy(mnt->type, "tmpfs");
    mnt->base.type = UNIT_TYPE_MOUNT;
    mnt->base.state = UNIT_STATE_DEAD;
    mnt->base.load_time = time_get_timestamp();
    list_add_tail(&mnt->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &mnt->base;
    
    slc = &systemd.slices[systemd.slice_count++];
    memset(slc, 0, sizeof(slice_t));
    INIT_LIST_HEAD(&slc->base.unit_list);
    INIT_LIST_HEAD(&slc->children);
    INIT_LIST_HEAD(&slc->tasks);
    strcpy(slc->base.name, "system");
    strcpy(slc->base.description, "System Slice");
    slc->type = SLICE_TYPE_SYSTEM;
    slc->base.type = UNIT_TYPE_SLICE;
    slc->base.state = UNIT_STATE_DEAD;
    slc->base.load_time = time_get_timestamp();
    list_add_tail(&slc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &slc->base;
    
    slc = &systemd.slices[systemd.slice_count++];
    memset(slc, 0, sizeof(slice_t));
    INIT_LIST_HEAD(&slc->base.unit_list);
    INIT_LIST_HEAD(&slc->children);
    INIT_LIST_HEAD(&slc->tasks);
    strcpy(slc->base.name, "user");
    strcpy(slc->base.description, "User Slice");
    slc->type = SLICE_TYPE_USER;
    slc->base.type = UNIT_TYPE_SLICE;
    slc->base.state = UNIT_STATE_DEAD;
    slc->base.load_time = time_get_timestamp();
    list_add_tail(&slc->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &slc->base;
    
    journal_log("systemd", "Built-in units loaded", LOG_INFO);
}

static int load_unit_file(const char* path)
{
    const char* ext = strrchr(path, '.');
    if (!ext) return -1;
    
    if (strcmp(ext, ".service") == 0) {
        return service_load(path);
    } else if (strcmp(ext, ".target") == 0) {
        return target_load(path);
    } else if (strcmp(ext, ".socket") == 0) {
        return socket_load(path);
    } else if (strcmp(ext, ".timer") == 0) {
        return timer_load(path);
    } else if (strcmp(ext, ".mount") == 0) {
        return mount_load(path);
    } else if (strcmp(ext, ".slice") == 0) {
        return slice_load(path);
    } else if (strcmp(ext, ".device") == 0) {
        return device_load(path);
    } else if (strcmp(ext, ".automount") == 0) {
        return automount_load(path);
    } else if (strcmp(ext, ".path") == 0) {
        return path_load(path);
    } else if (strcmp(ext, ".swap") == 0) {
        return swap_load(path);
    }
    
    return -1;
}

int systemd_load_units_from_dir(const char* dir)
{
    char entries[8192];
    int ret = fs_list_dir(dir, entries, sizeof(entries));
    if (ret <= 0) {
        journal_log("systemd", "No units found in directory", LOG_DEBUG);
        return 0;
    }
    
    char* line = entries;
    char* next;
    
    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';
        
        if (strlen(line) == 0) {
            line = next + 1;
            continue;
        }
        
        char path[SYSTEMD_MAX_PATH_LEN];
        sprintf(path, "%s/%s", dir, line);
        
        load_unit_file(path);
        
        line = next + 1;
    }
    
    return 0;
}

static bool systemd_detect_cycle(unit_t* unit, unit_t** visited, int visited_count)
{
    for (int i = 0; i < visited_count; i++) {
        if (visited[i] == unit) {
            char msg[256];
            sprintf(msg, "Detected dependency cycle involving %s", unit->name);
            journal_log("systemd", msg, LOG_ERR);
            return true;
        }
    }
    
    visited[visited_count] = unit;
    
    for (int i = 0; i < unit->after_count; i++) {
        unit_t* dep = find_unit(unit->after[i].name);
        if (dep) {
            if (systemd_detect_cycle(dep, visited, visited_count + 1)) {
                return true;
            }
        }
    }
    
    for (int i = 0; i < unit->requires_count; i++) {
        unit_t* dep = find_unit(unit->requires[i].name);
        if (dep) {
            if (systemd_detect_cycle(dep, visited, visited_count + 1)) {
                return true;
            }
        }
    }
    
    return false;
}

static int systemd_resolve_dependencies(unit_t* unit)
{
    if (!unit) return 0;
    
    for (int i = 0; i < unit->after_count; i++) {
        unit_t* dep = find_unit(unit->after[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD) {
            systemd_start_unit(dep->name);
        }
    }
    
    for (int i = 0; i < unit->requires_count; i++) {
        unit_t* dep = find_unit(unit->requires[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD) {
            systemd_start_unit(dep->name);
        }
    }
    
    for (int i = 0; i < unit->wants_count; i++) {
        unit_t* dep = find_unit(unit->wants[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD && dep->enabled) {
            systemd_start_unit(dep->name);
        }
    }
    
    for (int i = 0; i < unit->binds_to_count; i++) {
        unit_t* dep = find_unit(unit->binds_to[i].name);
        if (dep && dep->state == UNIT_STATE_DEAD) {
            systemd_start_unit(dep->name);
        }
    }
    
    return 0;
}

static void systemd_topological_sort(unit_t** units, int count, unit_t** sorted, int* sorted_count)
{
    int* in_degree = (int*)malloc(count * sizeof(int));
    if (!in_degree) return;
    
    memset(in_degree, 0, count * sizeof(int));
    
    for (int i = 0; i < count; i++) {
        unit_t* unit = units[i];
        for (int j = 0; j < unit->before_count; j++) {
            for (int k = 0; k < count; k++) {
                if (strcmp(units[k]->name, unit->before[j].name) == 0) {
                    in_degree[k]++;
                    break;
                }
            }
        }
    }
    
    int* queue = (int*)malloc(count * sizeof(int));
    int front = 0, rear = 0;
    
    for (int i = 0; i < count; i++) {
        if (in_degree[i] == 0) {
            queue[rear++] = i;
        }
    }
    
    *sorted_count = 0;
    
    while (front < rear) {
        int idx = queue[front++];
        sorted[*sorted_count] = units[idx];
        (*sorted_count)++;
        
        unit_t* unit = units[idx];
        for (int j = 0; j < unit->after_count; j++) {
            for (int k = 0; k < count; k++) {
                if (strcmp(units[k]->name, unit->after[j].name) == 0) {
                    in_degree[k]--;
                    if (in_degree[k] == 0) {
                        queue[rear++] = k;
                    }
                    break;
                }
            }
        }
    }
    
    free(queue);
    free(in_degree);
}

static int systemd_compare_units(unit_t* a, unit_t* b)
{
    for (int i = 0; i < a->after_count; i++) {
        if (strcmp(a->after[i].name, b->name) == 0) {
            return -1;
        }
    }
    for (int i = 0; i < b->after_count; i++) {
        if (strcmp(b->after[i].name, a->name) == 0) {
            return 1;
        }
    }
    
    int a_weight = 0;
    int b_weight = 0;
    
    switch (a->type) {
        case UNIT_TYPE_SLICE:     a_weight = 5; break;
        case UNIT_TYPE_DEVICE:    a_weight = 8; break;
        case UNIT_TYPE_MOUNT:     a_weight = 10; break;
        case UNIT_TYPE_TARGET:    a_weight = 20; break;
        case UNIT_TYPE_SOCKET:    a_weight = 30; break;
        case UNIT_TYPE_SERVICE:   a_weight = 40; break;
        case UNIT_TYPE_TIMER:     a_weight = 50; break;
        case UNIT_TYPE_AUTOMOUNT: a_weight = 55; break;
        case UNIT_TYPE_PATH:      a_weight = 60; break;
        case UNIT_TYPE_SWAP:      a_weight = 65; break;
        default:                  a_weight = 100; break;
    }
    
    switch (b->type) {
        case UNIT_TYPE_SLICE:     b_weight = 5; break;
        case UNIT_TYPE_DEVICE:    b_weight = 8; break;
        case UNIT_TYPE_MOUNT:     b_weight = 10; break;
        case UNIT_TYPE_TARGET:    b_weight = 20; break;
        case UNIT_TYPE_SOCKET:    b_weight = 30; break;
        case UNIT_TYPE_SERVICE:   b_weight = 40; break;
        case UNIT_TYPE_TIMER:     b_weight = 50; break;
        case UNIT_TYPE_AUTOMOUNT: b_weight = 55; break;
        case UNIT_TYPE_PATH:      b_weight = 60; break;
        case UNIT_TYPE_SWAP:      b_weight = 65; break;
        default:                  b_weight = 100; break;
    }
    
    return a_weight - b_weight;
}

static void systemd_sort_units_by_type(unit_t** units, int count)
{
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (systemd_compare_units(units[i], units[j]) > 0) {
                unit_t* tmp = units[i];
                units[i] = units[j];
                units[j] = tmp;
            }
        }
    }
}

static int systemd_start_unit_internal(unit_t* unit)
{
    if (!unit) return -1;
    
    if (unit->state != UNIT_STATE_DEAD && unit->state != UNIT_STATE_LOADED) {
        return 0;
    }
    
    if (!unit->enabled) {
        return 0;
    }
    
    systemd_resolve_dependencies(unit);
    
    switch (unit->type) {
        case UNIT_TYPE_SERVICE:
            return service_start((service_t*)unit);
        case UNIT_TYPE_TARGET:
            return target_start((target_t*)unit);
        case UNIT_TYPE_SOCKET:
            return socket_start((systemd_socket_t*)unit);
        case UNIT_TYPE_TIMER:
            return timer_start((timer_t*)unit);
        case UNIT_TYPE_MOUNT:
            return mount_start((mount_t*)unit);
        case UNIT_TYPE_SLICE:
            return slice_start((slice_t*)unit);
        case UNIT_TYPE_DEVICE:
            return device_start((device_t*)unit);
        case UNIT_TYPE_AUTOMOUNT:
            return automount_start((automount_t*)unit);
        case UNIT_TYPE_PATH:
            return path_start((systemd_path_t*)unit);
        case UNIT_TYPE_SWAP:
            return swap_start((swap_t*)unit);
        default:
            return -1;
    }
}

static void systemd_start_units_in_order(unit_t** sorted_units, int count)
{
    for (int i = 0; i < count; i++) {
        unit_t* unit = sorted_units[i];
        
        if (unit->state != UNIT_STATE_DEAD && unit->state != UNIT_STATE_LOADED) {
            continue;
        }
        
        if (!unit->enabled) {
            continue;
        }
        
        journal_log("systemd", unit->name, LOG_DEBUG);
        
        systemd_start_unit_internal(unit);
        
        msleep(10);
    }
}

static void systemd_start_default_target(void)
{
    journal_log("systemd", "Starting default target", LOG_INFO);
    
    unit_t* sorted[SYSTEMD_MAX_UNITS];
    int sorted_count = 0;
    
    systemd_topological_sort(systemd.units, systemd.unit_count, sorted, &sorted_count);
    
    systemd_sort_units_by_type(sorted, sorted_count);
    
    systemd_start_units_in_order(sorted, sorted_count);
    
    target_t* target = find_target(systemd.default_target);
    if (target) {
        target_start(target);
    }
}

void systemd_init(void)
{
    memset(&systemd, 0, sizeof(systemd_t));
    spinlock_init(&systemd.lock);
    INIT_LIST_HEAD(&systemd.unit_list_head);
    
    vfs_mkdir("/etc/systemd", 0755);
    vfs_mkdir("/etc/systemd/system", 0755);
    vfs_mkdir("/run/systemd", 0755);
    vfs_mkdir("/run/systemd/watchdog", 0755);
    vfs_mkdir("/var/lib/systemd", 0755);
    
    journal_init(true);
    
    generate_boot_id();
    strcpy(systemd.manager_version, "252");
    
    init_cgroup_slices();
    
    fstab_load("/etc/fstab");
    
    udev_load_rules("/etc/udev/rules.d");
    /* udev_scan_devices(); */
    
    fstab_mount_all();
    
    systemd_load_units_from_dir("/etc/systemd/system");
    
    load_builtin_units();
    
    strcpy(systemd.default_target, "multi-user");
    strcpy(systemd.emergency_target, "emergency");
    strcpy(systemd.rescue_target, "rescue");
    
    systemd.initialized = true;
    
    journal_log("systemd", "Initialized", LOG_INFO);
}

void systemd_run(void)
{
    journal_log("systemd", "Starting systemd", LOG_INFO);
    
    systemd.running = true;
    
    systemd_start_default_target();
    
    while (systemd.running) {
        for (int i = 0; i < systemd.service_count; i++) {
            service_monitor(&systemd.services[i]);
        }
        
        for (int i = 0; i < systemd.timer_count; i++) {
            timer_monitor(&systemd.timers[i]);
        }
        
        msleep(500);
    }
}

void systemd_shutdown(void)
{
    journal_log("systemd", "Shutting down", LOG_INFO);
    
    systemd.shutting_down = true;
    
    for (int i = 0; i < systemd.service_count; i++) {
        service_t* service = &systemd.services[i];
        if (service->running) {
            service_stop(service);
        }
    }
    
    for (int i = 0; i < systemd.socket_count; i++) {
        systemd_socket_t* sock = &systemd.sockets[i];
        if (sock->base.state == UNIT_STATE_ACTIVE) {
            socket_stop(sock);
        }
    }
    
    for (int i = 0; i < systemd.mount_count; i++) {
        mount_t* mount = &systemd.mounts[i];
        if (mount->base.state == UNIT_STATE_ACTIVE) {
            mount_stop(mount);
        }
    }
    
    for (int i = 0; i < systemd.timer_count; i++) {
        timer_t* timer = &systemd.timers[i];
        if (timer->base.state == UNIT_STATE_ACTIVE) {
            timer_stop(timer);
        }
    }
    
    systemd.running = false;
    
    journal_log("systemd", "Shutdown complete", LOG_INFO);
}

int systemd_start_target(const char* name)
{
    target_t* target = find_target(name);
    if (!target) {
        journal_log("systemd", "Target not found", LOG_ERR);
        return -1;
    }
    
    return target_start(target);
}

int systemd_start_service(const char* name)
{
    service_t* service = find_service(name);
    if (!service) {
        journal_log("systemd", "Service not found", LOG_ERR);
        return -1;
    }
    
    return service_start(service);
}

int systemd_stop_service(const char* name)
{
    service_t* service = find_service(name);
    if (!service) {
        journal_log("systemd", "Service not found", LOG_ERR);
        return -1;
    }
    
    return service_stop(service);
}

int systemd_restart_service(const char* name)
{
    service_t* service = find_service(name);
    if (!service) {
        journal_log("systemd", "Service not found", LOG_ERR);
        return -1;
    }
    
    service_stop(service);
    
    if (service->restart_sec > 0) {
        msleep(service->restart_sec * 1000);
    }
    
    service_start(service);
    return 0;
}

int systemd_reload_service(const char* name)
{
    service_t* service = find_service(name);
    if (!service) {
        journal_log("systemd", "Service not found", LOG_ERR);
        return -1;
    }
    
    return service_reload(service);
}

int systemd_enable_service(const char* name)
{
    service_t* service = find_service(name);
    if (!service) {
        journal_log("systemd", "Service not found", LOG_ERR);
        return -1;
    }
    
    service->base.enabled = true;
    journal_log(service->base.name, "Enabled", LOG_INFO);
    return 0;
}

int systemd_disable_service(const char* name)
{
    service_t* service = find_service(name);
    if (!service) {
        journal_log("systemd", "Service not found", LOG_ERR);
        return -1;
    }
    
    service->base.enabled = false;
    journal_log(service->base.name, "Disabled", LOG_INFO);
    return 0;
}

int systemd_list_services(char* buffer, int size)
{
    if (!buffer || size <= 0) return -1;
    
    int pos = 0;
    
    for (int i = 0; i < systemd.service_count; i++) {
        service_t* service = &systemd.services[i];
        char line[256];
        const char* state_str = "unknown";
        switch (service->base.state) {
            case UNIT_STATE_DEAD:         state_str = "dead"; break;
            case UNIT_STATE_LOADING:      state_str = "loading"; break;
            case UNIT_STATE_LOADED:       state_str = "loaded"; break;
            case UNIT_STATE_ACTIVATING:   state_str = "activating"; break;
            case UNIT_STATE_ACTIVE:       state_str = "active"; break;
            case UNIT_STATE_RUNNING:      state_str = "running"; break;
            case UNIT_STATE_DEACTIVATING: state_str = "deactivating"; break;
            case UNIT_STATE_INACTIVE:     state_str = "inactive"; break;
            case UNIT_STATE_FAILED:       state_str = "failed"; break;
            case UNIT_STATE_EXITED:       state_str = "exited"; break;
        }
        sprintf(line, "%s (%s)\n", service->base.name, state_str);
        
        int len = strlen(line);
        if (pos + len >= size) break;
        
        strcpy(buffer + pos, line);
        pos += len;
    }
    
    buffer[pos] = '\0';
    return pos;
}

int systemd_get_service_status(const char* name, char* buffer, int size)
{
    service_t* service = find_service(name);
    if (!service || !buffer || size <= 0) return -1;
    
    const char* state_str = "unknown";
    switch (service->base.state) {
        case UNIT_STATE_DEAD:         state_str = "dead"; break;
        case UNIT_STATE_LOADING:      state_str = "loading"; break;
        case UNIT_STATE_LOADED:       state_str = "loaded"; break;
        case UNIT_STATE_ACTIVATING:   state_str = "activating"; break;
        case UNIT_STATE_ACTIVE:       state_str = "active"; break;
        case UNIT_STATE_RUNNING:      state_str = "running"; break;
        case UNIT_STATE_DEACTIVATING: state_str = "deactivating"; break;
        case UNIT_STATE_INACTIVE:     state_str = "inactive"; break;
        case UNIT_STATE_FAILED:       state_str = "failed"; break;
        case UNIT_STATE_EXITED:       state_str = "exited"; break;
    }
    
    int pos = sprintf(buffer, "● %s\n", service->base.name);
    pos += sprintf(buffer + pos, "   State: %s\n", state_str);
    pos += sprintf(buffer + pos, "   PID: %llu\n", service->pid);
    pos += sprintf(buffer + pos, "   Description: %s\n", service->base.description);
    pos += sprintf(buffer + pos, "   Type: ");
    switch (service->type) {
        case SERVICE_TYPE_SIMPLE: pos += sprintf(buffer + pos, "simple\n"); break;
        case SERVICE_TYPE_FORKING: pos += sprintf(buffer + pos, "forking\n"); break;
        case SERVICE_TYPE_ONESHOT: pos += sprintf(buffer + pos, "oneshot\n"); break;
        case SERVICE_TYPE_DBUS: pos += sprintf(buffer + pos, "dbus\n"); break;
        case SERVICE_TYPE_NOTIFY: pos += sprintf(buffer + pos, "notify\n"); break;
        case SERVICE_TYPE_IDLE: pos += sprintf(buffer + pos, "idle\n"); break;
        default: pos += sprintf(buffer + pos, "unknown\n"); break;
    }
    pos += sprintf(buffer + pos, "   Restart: ");
    switch (service->restart) {
        case RESTART_NO: pos += sprintf(buffer + pos, "no\n"); break;
        case RESTART_ALWAYS: pos += sprintf(buffer + pos, "always\n"); break;
        case RESTART_ON_SUCCESS: pos += sprintf(buffer + pos, "on-success\n"); break;
        case RESTART_ON_FAILURE: pos += sprintf(buffer + pos, "on-failure\n"); break;
        case RESTART_ON_ABNORMAL: pos += sprintf(buffer + pos, "on-abnormal\n"); break;
        case RESTART_ON_WATCHDOG: pos += sprintf(buffer + pos, "on-watchdog\n"); break;
        case RESTART_ON_ABORT: pos += sprintf(buffer + pos, "on-abort\n"); break;
        default: pos += sprintf(buffer + pos, "unknown\n"); break;
    }
    pos += sprintf(buffer + pos, "   RestartSec: %llus\n", service->restart_sec);
    
    return pos;
}

unit_state_t systemd_get_service_state(const char* name)
{
    service_t* service = find_service(name);
    if (!service) return UNIT_STATE_DEAD;
    
    return service->base.state;
}

int systemd_isolate_target(const char* name)
{
    target_t* target = find_target(name);
    if (!target) {
        journal_log("systemd", "Target not found", LOG_ERR);
        return -1;
    }
    
    for (int i = 0; i < systemd.service_count; i++) {
        service_t* service = &systemd.services[i];
        if (service->running) {
            service_stop(service);
        }
    }
    
    for (int i = 0; i < systemd.socket_count; i++) {
        systemd_socket_t* sock = &systemd.sockets[i];
        if (sock->base.state == UNIT_STATE_ACTIVE) {
            socket_stop(sock);
        }
    }
    
    systemd_resolve_dependencies(&target->base);
    target_start(target);
    
    strcpy(systemd.default_target, name);
    
    journal_log("systemd", "Isolated to target", LOG_INFO);
    return 0;
}

int systemd_start_unit(const char* name)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    return systemd_start_unit_internal(unit);
}

int systemd_stop_unit(const char* name)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    switch (unit->type) {
        case UNIT_TYPE_SERVICE:
            return service_stop((service_t*)unit);
        case UNIT_TYPE_SOCKET:
            return socket_stop((systemd_socket_t*)unit);
        case UNIT_TYPE_TIMER:
            return timer_stop((timer_t*)unit);
        case UNIT_TYPE_MOUNT:
            return mount_stop((mount_t*)unit);
        default:
            return -1;
    }
}

int systemd_reload_unit(const char* name)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    if (unit->type == UNIT_TYPE_SERVICE) {
        return service_reload((service_t*)unit);
    }
    
    return -1;
}

int systemd_restart_unit(const char* name)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    if (unit->type == UNIT_TYPE_SERVICE) {
        service_t* service = (service_t*)unit;
        service_stop(service);
        if (service->restart_sec > 0) {
            msleep(service->restart_sec * 1000);
        }
        service_start(service);
        return 0;
    }
    
    return -1;
}

int systemd_list_units(char* buffer, int size)
{
    if (!buffer || size <= 0) return -1;
    
    int pos = 0;
    
    for (int i = 0; i < systemd.unit_count; i++) {
        unit_t* unit = systemd.units[i];
        char line[256];
        const char* type_str = "unknown";
        switch (unit->type) {
            case UNIT_TYPE_SERVICE: type_str = "service"; break;
            case UNIT_TYPE_TARGET: type_str = "target"; break;
            case UNIT_TYPE_SOCKET: type_str = "socket"; break;
            case UNIT_TYPE_TIMER: type_str = "timer"; break;
            case UNIT_TYPE_MOUNT: type_str = "mount"; break;
            case UNIT_TYPE_SLICE: type_str = "slice"; break;
            case UNIT_TYPE_DEVICE: type_str = "device"; break;
            case UNIT_TYPE_AUTOMOUNT: type_str = "automount"; break;
            case UNIT_TYPE_PATH: type_str = "path"; break;
            case UNIT_TYPE_SWAP: type_str = "swap"; break;
            default: type_str = "unknown"; break;
        }
        const char* state_str = "unknown";
        switch (unit->state) {
            case UNIT_STATE_DEAD: state_str = "dead"; break;
            case UNIT_STATE_LOADING: state_str = "loading"; break;
            case UNIT_STATE_LOADED: state_str = "loaded"; break;
            case UNIT_STATE_ACTIVATING: state_str = "activating"; break;
            case UNIT_STATE_ACTIVE: state_str = "active"; break;
            case UNIT_STATE_RUNNING: state_str = "running"; break;
            case UNIT_STATE_DEACTIVATING: state_str = "deactivating"; break;
            case UNIT_STATE_INACTIVE: state_str = "inactive"; break;
            case UNIT_STATE_FAILED: state_str = "failed"; break;
            case UNIT_STATE_EXITED: state_str = "exited"; break;
            default: state_str = "unknown"; break;
        }
        sprintf(line, "%s (%s) %s\n", unit->name, type_str, state_str);
        
        int len = strlen(line);
        if (pos + len >= size) break;
        
        strcpy(buffer + pos, line);
        pos += len;
    }
    
    buffer[pos] = '\0';
    return pos;
}

int systemd_list_units_by_type(char* buffer, int size, unit_type_t type)
{
    if (!buffer || size <= 0) return -1;
    
    int pos = 0;
    
    for (int i = 0; i < systemd.unit_count; i++) {
        unit_t* unit = systemd.units[i];
        if (unit->type != type) continue;
        
        char line[256];
        const char* state_str = "unknown";
        switch (unit->state) {
            case UNIT_STATE_DEAD: state_str = "dead"; break;
            case UNIT_STATE_LOADING: state_str = "loading"; break;
            case UNIT_STATE_LOADED: state_str = "loaded"; break;
            case UNIT_STATE_ACTIVATING: state_str = "activating"; break;
            case UNIT_STATE_ACTIVE: state_str = "active"; break;
            case UNIT_STATE_RUNNING: state_str = "running"; break;
            case UNIT_STATE_DEACTIVATING: state_str = "deactivating"; break;
            case UNIT_STATE_INACTIVE: state_str = "inactive"; break;
            case UNIT_STATE_FAILED: state_str = "failed"; break;
            case UNIT_STATE_EXITED: state_str = "exited"; break;
            default: state_str = "unknown"; break;
        }
        sprintf(line, "%s (%s)\n", unit->name, state_str);
        
        int len = strlen(line);
        if (pos + len >= size) break;
        
        strcpy(buffer + pos, line);
        pos += len;
    }
    
    buffer[pos] = '\0';
    return pos;
}

int systemd_daemon_reload(void)
{
    journal_log("systemd", "Reloading daemon", LOG_INFO);
    
    systemd.unit_count = 0;
    systemd.service_count = 0;
    systemd.target_count = 0;
    systemd.socket_count = 0;
    systemd.timer_count = 0;
    systemd.mount_count = 0;
    systemd.slice_count = 0;
    systemd.device_count = 0;
    systemd.automount_count = 0;
    systemd.path_count = 0;
    systemd.swap_count = 0;
    
    systemd_load_units_from_dir("/etc/systemd/system");
    load_builtin_units();
    
    journal_log("systemd", "Daemon reloaded", LOG_INFO);
    return 0;
}

int systemd_kill_unit(const char* name, int signal)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    if (unit->type == UNIT_TYPE_SERVICE) {
        return service_kill((service_t*)unit, signal);
    }
    
    return -1;
}

int systemd_show_unit(const char* name, char* buffer, int size)
{
    unit_t* unit = find_unit(name);
    if (!unit || !buffer || size <= 0) return -1;
    
    int pos = 0;
    
    pos += sprintf(buffer + pos, "Unit: %s\n", unit->name);
    pos += sprintf(buffer + pos, "Type: ");
    switch (unit->type) {
        case UNIT_TYPE_SERVICE: pos += sprintf(buffer + pos, "service\n"); break;
        case UNIT_TYPE_TARGET: pos += sprintf(buffer + pos, "target\n"); break;
        case UNIT_TYPE_SOCKET: pos += sprintf(buffer + pos, "socket\n"); break;
        case UNIT_TYPE_TIMER: pos += sprintf(buffer + pos, "timer\n"); break;
        case UNIT_TYPE_MOUNT: pos += sprintf(buffer + pos, "mount\n"); break;
        case UNIT_TYPE_SLICE: pos += sprintf(buffer + pos, "slice\n"); break;
        case UNIT_TYPE_DEVICE: pos += sprintf(buffer + pos, "device\n"); break;
        case UNIT_TYPE_AUTOMOUNT: pos += sprintf(buffer + pos, "automount\n"); break;
        case UNIT_TYPE_PATH: pos += sprintf(buffer + pos, "path\n"); break;
        case UNIT_TYPE_SWAP: pos += sprintf(buffer + pos, "swap\n"); break;
        default: pos += sprintf(buffer + pos, "unknown\n"); break;
    }
    pos += sprintf(buffer + pos, "State: ");
    switch (unit->state) {
        case UNIT_STATE_DEAD: pos += sprintf(buffer + pos, "dead\n"); break;
        case UNIT_STATE_LOADING: pos += sprintf(buffer + pos, "loading\n"); break;
        case UNIT_STATE_LOADED: pos += sprintf(buffer + pos, "loaded\n"); break;
        case UNIT_STATE_ACTIVATING: pos += sprintf(buffer + pos, "activating\n"); break;
        case UNIT_STATE_ACTIVE: pos += sprintf(buffer + pos, "active\n"); break;
        case UNIT_STATE_RUNNING: pos += sprintf(buffer + pos, "running\n"); break;
        case UNIT_STATE_DEACTIVATING: pos += sprintf(buffer + pos, "deactivating\n"); break;
        case UNIT_STATE_INACTIVE: pos += sprintf(buffer + pos, "inactive\n"); break;
        case UNIT_STATE_FAILED: pos += sprintf(buffer + pos, "failed\n"); break;
        case UNIT_STATE_EXITED: pos += sprintf(buffer + pos, "exited\n"); break;
        default: pos += sprintf(buffer + pos, "unknown\n"); break;
    }
    pos += sprintf(buffer + pos, "Description: %s\n", unit->description);
    pos += sprintf(buffer + pos, "Loaded: %s (%s)\n", 
                   unit->enabled ? "yes" : "no", unit->source_path);
    
    if (unit->wants_count > 0) {
        pos += sprintf(buffer + pos, "Wants: ");
        for (int i = 0; i < unit->wants_count; i++) {
            if (i > 0) pos += sprintf(buffer + pos, ", ");
            pos += sprintf(buffer + pos, "%s", unit->wants[i].name);
        }
        pos += sprintf(buffer + pos, "\n");
    }
    
    if (unit->requires_count > 0) {
        pos += sprintf(buffer + pos, "Requires: ");
        for (int i = 0; i < unit->requires_count; i++) {
            if (i > 0) pos += sprintf(buffer + pos, ", ");
            pos += sprintf(buffer + pos, "%s", unit->requires[i].name);
        }
        pos += sprintf(buffer + pos, "\n");
    }
    
    if (unit->after_count > 0) {
        pos += sprintf(buffer + pos, "After: ");
        for (int i = 0; i < unit->after_count; i++) {
            if (i > 0) pos += sprintf(buffer + pos, ", ");
            pos += sprintf(buffer + pos, "%s", unit->after[i].name);
        }
        pos += sprintf(buffer + pos, "\n");
    }
    
    if (unit->before_count > 0) {
        pos += sprintf(buffer + pos, "Before: ");
        for (int i = 0; i < unit->before_count; i++) {
            if (i > 0) pos += sprintf(buffer + pos, ", ");
            pos += sprintf(buffer + pos, "%s", unit->before[i].name);
        }
        pos += sprintf(buffer + pos, "\n");
    }
    
    buffer[pos] = '\0';
    return pos;
}

int systemd_enable_unit(const char* name)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    unit->enabled = true;
    journal_log(unit->name, "Enabled", LOG_INFO);
    return 0;
}

int systemd_disable_unit(const char* name)
{
    unit_t* unit = find_unit(name);
    if (!unit) {
        journal_log("systemd", "Unit not found", LOG_ERR);
        return -1;
    }
    
    unit->enabled = false;
    journal_log(unit->name, "Disabled", LOG_INFO);
    return 0;
}

int systemd_reload_config(void)
{
    return systemd_daemon_reload();
}

int systemd_isolate(const char* name)
{
    return systemd_isolate_target(name);
}