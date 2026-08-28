#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>
#include <unixsock.h>

static socket_type_t parse_socket_type(const char* str)
{
    if (strcmp(str, "dgram") == 0) return SOCKET_TYPE_DGRAM;
    if (strcmp(str, "seqpacket") == 0) return SOCKET_TYPE_SEQPACKET;
    if (strcmp(str, "raw") == 0) return SOCKET_TYPE_RAW;
    if (strcmp(str, "rdm") == 0) return SOCKET_TYPE_RDM;
    return SOCKET_TYPE_STREAM;
}

static socket_protocol_t parse_socket_protocol(const char* str)
{
    if (strcmp(str, "udp") == 0) return SOCKET_PROTOCOL_UDP;
    if (strcmp(str, "unix") == 0) return SOCKET_PROTOCOL_UNIX;
    return SOCKET_PROTOCOL_TCP;
}

static int parse_socket_config(const char* path, systemd_socket_t* socket)
{
    char content[8192];
    int ret = fs_read_file_content(path, content, sizeof(content));
    if (ret <= 0) {
        return -1;
    }
    
    char section[64] = "";
    char key[128], value[1024];
    char* line = content;
    char* next;
    
    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';
        trim(line);
        if (*line == '\0' || *line == '#') {
            line = next + 1;
            continue;
        }
        if (line[0] == '[' && parse_ini_section(line, section, sizeof(section)) == 0) {
            line = next + 1;
            continue;
        }
        if (parse_ini_keyvalue(line, key, sizeof(key), value, sizeof(value)) != 0) {
            line = next + 1;
            continue;
        }
        
        if (strcmp(section, "Socket") == 0) {
            if (strcmp(key, "ListenStream") == 0) {
                strncpy(socket->socket_path, value, SYSTEMD_MAX_PATH - 1);
                socket->listen_stream = true;
                socket->type = SOCKET_TYPE_STREAM;
            } else if (strcmp(key, "ListenDatagram") == 0) {
                strncpy(socket->socket_path, value, SYSTEMD_MAX_PATH - 1);
                socket->listen_datagram = true;
                socket->type = SOCKET_TYPE_DGRAM;
            } else if (strcmp(key, "ListenPort") == 0) {
                socket->port = atoi(value);
            } else if (strcmp(key, "ListenAddress") == 0) {
                strncpy(socket->listen_addr, value, 255);
            } else if (strcmp(key, "SocketType") == 0) {
                socket->type = parse_socket_type(value);
            } else if (strcmp(key, "Protocol") == 0) {
                socket->protocol = parse_socket_protocol(value);
            } else if (strcmp(key, "Accept") == 0) {
                socket->accept = (strcmp(value, "yes") == 0);
            } else if (strcmp(key, "Backlog") == 0) {
                socket->backlog = atoi(value);
            }
        } else if (strcmp(section, "Unit") == 0) {
            if (strcmp(key, "Description") == 0) {
                strncpy(socket->base.description, value, SYSTEMD_MAX_DESCRIPTION - 1);
            }
        } else if (strcmp(section, "Install") == 0) {
            if (strcmp(key, "WantedBy") == 0) {
                socket->base.enabled = true;
            }
        }
        
        line = next + 1;
    }
    
    return 0;
}

static void socket_accept_thread(void* arg)
{
    systemd_socket_t* socket = (systemd_socket_t*)arg;
    if (!socket || !socket->unix_sock) return;
    
    char msg[256];
    sprintf(msg, "Accept thread started for %s", socket->base.name);
    journal_log("systemd", msg, LOG_DEBUG);
    
    while (socket->base.state == UNIT_STATE_ACTIVE) {
        unix_sock_t* client_sock = unix_socket_accept(socket->unix_sock);
        if (!client_sock) {
            msleep(100);
            continue;
        }
        
        char msg[256];
        sprintf(msg, "Connection accepted on %s", socket->socket_path);
        journal_log(socket->base.name, msg, LOG_DEBUG);
        
        socket_activate(socket);
        
        unix_socket_close(client_sock);
    }
}

int socket_load(const char* path)
{
    if (systemd.socket_count >= SYSTEMD_MAX_SOCKETS) return -1;
    
    systemd_socket_t* socket = &systemd.sockets[systemd.socket_count];
    memset(socket, 0, sizeof(systemd_socket_t));
    INIT_LIST_HEAD(&socket->base.unit_list);
    INIT_LIST_HEAD(&socket->service_link);
    
    strncpy(socket->base.source_path, path, SYSTEMD_MAX_PATH - 1);
    
    char* filename = strrchr(path, '/');
    if (!filename) filename = (char*)path;
    else filename++;
    
    char* dot = strchr(filename, '.');
    if (dot) *dot = '\0';
    strncpy(socket->base.name, filename, SYSTEMD_MAX_NAME - 1);
    if (dot) *dot = '.';
    
    strcpy(socket->service_name, socket->base.name);
    
    parse_socket_config(path, socket);
    
    socket->base.type = UNIT_TYPE_SOCKET;
    socket->base.state = UNIT_STATE_DEAD;
    socket->unix_sock = NULL;
    socket->accept_thread_id = -1;
    socket->backlog = (socket->backlog == 0) ? 16 : socket->backlog;
    
    socket->base.load_time = time_get_timestamp();
    
    list_add_tail(&socket->base.unit_list, &systemd.unit_list_head);
    systemd.units[systemd.unit_count++] = &socket->base;
    systemd.socket_count++;
    
    journal_log(socket->base.name, "Socket loaded", LOG_INFO);
    
    if (socket->base.enabled) {
        socket_start(socket);
    }
    
    return 0;
}

int socket_start(systemd_socket_t* socket)
{
    if (!socket) return -1;
    
    spinlock_lock(&systemd.lock);
    socket->base.state = UNIT_STATE_ACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(socket->base.name, "Starting socket", LOG_INFO);
    
    int unix_type = (socket->type == SOCKET_TYPE_DGRAM) ? UNIX_SOCK_DGRAM : UNIX_SOCK_STREAM;
    socket->unix_sock = unix_socket_create(unix_type);
    if (!socket->unix_sock) {
        char msg[256];
        sprintf(msg, "Failed to create socket for %s", socket->base.name);
        journal_log("systemd", msg, LOG_ERR);
        spinlock_lock(&systemd.lock);
        socket->base.state = UNIT_STATE_FAILED;
        spinlock_unlock(&systemd.lock);
        return -1;
    }
    
    if (strlen(socket->socket_path) > 0) {
        vfs_unlink(socket->socket_path);
        
        int ret = unix_socket_bind(socket->unix_sock, socket->socket_path);
        if (ret != 0) {
            char msg[256];
            sprintf(msg, "Failed to bind socket %s", socket->socket_path);
            journal_log("systemd", msg, LOG_ERR);
            unix_socket_close(socket->unix_sock);
            socket->unix_sock = NULL;
            spinlock_lock(&systemd.lock);
            socket->base.state = UNIT_STATE_FAILED;
            spinlock_unlock(&systemd.lock);
            return -1;
        }
        
        char msg[256];
        sprintf(msg, "Bound socket to %s", socket->socket_path);
        journal_log(socket->base.name, msg, LOG_INFO);
    }
    
    if (socket->type == SOCKET_TYPE_STREAM) {
        int ret = unix_socket_listen(socket->unix_sock, socket->backlog);
        if (ret != 0) {
            char msg[256];
            sprintf(msg, "Failed to listen on socket %s", socket->socket_path);
            journal_log("systemd", msg, LOG_ERR);
            unix_socket_close(socket->unix_sock);
            socket->unix_sock = NULL;
            spinlock_lock(&systemd.lock);
            socket->base.state = UNIT_STATE_FAILED;
            spinlock_unlock(&systemd.lock);
            return -1;
        }
        
        char msg[256];
        sprintf(msg, "Listening on socket %s", socket->socket_path);
        journal_log(socket->base.name, msg, LOG_INFO);
        
        socket->accept_thread_id = thread_create((void*)(unsigned long)socket_accept_thread, socket);
        if (socket->accept_thread_id < 0) {
            char msg[256];
            sprintf(msg, "Failed to create accept thread for %s", socket->base.name);
            journal_log("systemd", msg, LOG_WARNING);
        }
    }
    
    spinlock_lock(&systemd.lock);
    socket->base.state = UNIT_STATE_ACTIVE;
    socket->base.active_time = time_get_timestamp();
    socket->activated = true;
    spinlock_unlock(&systemd.lock);
    
    return 0;
}

int socket_stop(systemd_socket_t* socket)
{
    if (!socket) return -1;
    
    spinlock_lock(&systemd.lock);
    socket->base.state = UNIT_STATE_DEACTIVATING;
    spinlock_unlock(&systemd.lock);
    
    journal_log(socket->base.name, "Stopping socket", LOG_INFO);
    
    if (socket->unix_sock) {
        unix_socket_close(socket->unix_sock);
        socket->unix_sock = NULL;
    }
    
    if (strlen(socket->socket_path) > 0) {
        vfs_unlink(socket->socket_path);
    }
    
    spinlock_lock(&systemd.lock);
    socket->base.state = UNIT_STATE_DEAD;
    socket->base.inactive_time = time_get_timestamp();
    socket->activated = false;
    spinlock_unlock(&systemd.lock);
    
    journal_log(socket->base.name, "Socket stopped", LOG_INFO);
    
    return 0;
}

int socket_listen(systemd_socket_t* socket)
{
    return socket_start(socket);
}

int socket_activate(systemd_socket_t* socket)
{
    if (!socket) return -1;
    
    char msg[256];
    sprintf(msg, "Socket activated, starting service %s", socket->service_name);
    journal_log(socket->base.name, msg, LOG_INFO);
    
    for (int i = 0; i < systemd.service_count; i++) {
        service_t* service = &systemd.services[i];
        if (strcmp(service->base.name, socket->service_name) == 0) {
            if (service->base.state == UNIT_STATE_DEAD) {
                service_start(service);
            } else if (service->base.state == UNIT_STATE_EXITED) {
                service_start(service);
            }
            break;
        }
    }
    
    return 0;
}