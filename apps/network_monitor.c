#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define MAX_CONNECTIONS 32
#define MAX_HOST_LEN 64

typedef struct {
    char local_addr[32];
    int local_port;
    char remote_addr[32];
    int remote_port;
    char state[16];
    char protocol[8];
    int pid;
    char program[MAX_HOST_LEN];
    long bytes_sent;
    long bytes_recv;
} Connection;

static Connection connections[MAX_CONNECTIONS];
static int connection_count = 0;
static int selected_conn = 0;

static void generate_connections(void) {
    const char* states[] = {"ESTABLISHED", "LISTEN", "TIME_WAIT", "CLOSE_WAIT"};
    const char* protocols[] = {"TCP", "UDP", "TCP6", "UDP6"};
    const char* programs[] = {"firefox", "ssh", "systemd", "dnsmasq", 
                              "containerd", "kex-network", "browser", "terminal"};
    const char* remote_hosts[] = {
        "203.0.113.42", "198.51.100.23", "192.168.1.1", "8.8.8.8",
        "1.1.1.1", "142.250.185.78", "157.240.1.35", "52.94.236.248"
    };
    
    connection_count = 16 + (rand() % 8);
    if (connection_count > MAX_CONNECTIONS) connection_count = MAX_CONNECTIONS;
    
    for (int i = 0; i < connection_count; i++) {
        snprintf(connections[i].local_addr, sizeof(connections[i].local_addr), 
                 "192.168.1.100");
        connections[i].local_port = 30000 + rand() % 35000;
        
        if (i < 3 || rand() % 4 == 0) {
            strcpy(connections[i].state, "LISTEN");
            memset(connections[i].remote_addr, 0, sizeof(connections[i].remote_addr));
            connections[i].remote_port = 0;
        } else {
            strcpy(connections[i].state, states[rand() % 4]);
            strncpy(connections[i].remote_addr, 
                    remote_hosts[rand() % (sizeof(remote_hosts)/sizeof(remote_hosts[0]))],
                    sizeof(connections[i].remote_addr) - 1);
            connections[i].remote_port = 80 + rand() % 8000;
        }
        
        strcpy(connections[i].protocol, protocols[rand() % 4]);
        connections[i].pid = 1000 + rand() % 9000;
        strncpy(connections[i].program, programs[rand() % (sizeof(programs)/sizeof(programs[0]))],
                MAX_HOST_LEN - 1);
        
        connections[i].bytes_sent = (long)(rand() % 1000000);
        connections[i].bytes_recv = (long)(rand() % 2000000);
    }
}

static void format_bytes(long bytes, char* buf, size_t size) {
    if (bytes < 1024) {
        snprintf(buf, size, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, size, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, size, "%.1f MB", bytes / (1024.0 * 1024));
    } else {
        snprintf(buf, size, "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    }
}

static void draw_header(void) {
    vga_clear();
    
    vga_setcolor(0x0B, 0);
    vga_print("=== Kenux Network Monitor ===\n\n");
    
    vga_setcolor(0x09, 0);
    printf(" %-5s %-18s %-8s %-18s %-8s %-12s %-6s %-14s\n",
           "Proto", "Local Address", "Port", "Remote Address", "Port", 
           "State", "PID", "Program");
    
    vga_setcolor(0x07, 0);
    vga_print("----- ------------------ -------- ------------------ -------- "
              "------------ ------ --------------\n");
}

static void draw_connections(void) {
    int max_visible = 14;
    int start = 0;
    
    if (selected_conn >= max_visible) {
        start = selected_conn - max_visible + 1;
    }
    
    int end = start + max_visible;
    if (end > connection_count) end = connection_count;
    
    for (int i = start; i < end; i++) {
        if (i == selected_conn) {
            vga_setcolor(0x70, 0);
        } else {
            vga_setcolor(0x07, 0);
        }
        
        char sent_str[16], recv_str[16];
        format_bytes(connections[i].bytes_sent, sent_str, sizeof(sent_str));
        format_bytes(connections[i].bytes_recv, recv_str, sizeof(recv_str));
        
        if (strlen(connections[i].remote_addr) > 0) {
            printf(" %-5s %-17s %-7d %-17s %-7d %-11s %-5d %-13s\n",
                   connections[i].protocol,
                   connections[i].local_addr,
                   connections[i].local_port,
                   connections[i].remote_addr,
                   connections[i].remote_port,
                   connections[i].state,
                   connections[i].pid,
                   connections[i].program);
        } else {
            printf(" %-5s %-17s %-7d %-18s %-7s %-11s %-5d %-13s\n",
                   connections[i].protocol,
                   connections[i].local_addr,
                   connections[i].local_port,
                   "*:*",
                   "",
                   connections[i].state,
                   connections[i].pid,
                   connections[i].program);
        }
    }
}

static void draw_statistics(void) {
    long total_sent = 0, total_recv = 0;
    int tcp_count = 0, udp_count = 0, listen_count = 0;
    
    for (int i = 0; i < connection_count; i++) {
        total_sent += connections[i].bytes_sent;
        total_recv += connections[i].bytes_recv;
        
        if (strncmp(connections[i].protocol, "TCP", 3) == 0) tcp_count++;
        else udp_count++;
        
        if (strcmp(connections[i].state, "LISTEN") == 0) listen_count++;
    }
    
    vga_setcolor(0x0E, 0);
    vga_print("\n=== Network Statistics ===\n\n");
    
    vga_setcolor(0x0F, 0);
    char stats[256];
    
    char total_sent_str[16], total_recv_str[16];
    format_bytes(total_sent, total_sent_str, sizeof(total_sent_str));
    format_bytes(total_recv, total_recv_str, sizeof(total_recv_str));
    
    sprintf(stats, "Total Connections: %d (TCP: %d, UDP: %d)\n", 
            connection_count, tcp_count, udp_count);
    vga_print(stats);
    
    sprintf(stats, "Listening Ports:   %d\n", listen_count);
    vga_print(stats);
    
    sprintf(stats, "Total Sent:        %s\n", total_sent_str);
    vga_print(stats);
    
    sprintf(stats, "Total Received:    %s\n", total_recv_str);
    vga_print(stats);
    
    sprintf(stats, "Interface:         eth0 (UP, 1000Mbps Full-Duplex)\n");
    vga_print(stats);
    
    sprintf(stats, "IP Address:        192.168.1.100/24\n");
    vga_print(stats);
    
    sprintf(stats, "MAC Address:       AA:BB:CC:DD:EE:FF\n");
    vga_print(stats);
    
    sprintf(stats, "DNS Server:        192.168.1.1, 8.8.8.8\n");
    vga_print(stats);
    
    vga_setcolor(0x0A, 0);
    vga_print("\nControls:\n");
    vga_print("Up/Down: Navigate | D: Show Details | R: Refresh | Q: Quit\n");
}

static void draw_connection_detail(int idx) {
    if (idx < 0 || idx >= connection_count) return;
    
    Connection* c = &connections[idx];
    
    vga_setcolor(0x0E, 0);
    vga_print("\n=== Connection Details ===\n\n");
    
    vga_setcolor(0x0F, 0);
    char detail[128];
    
    sprintf(detail, "Protocol:         %s\n", c->protocol); vga_print(detail);
    sprintf(detail, "Local Address:    %s:%d\n", c->local_addr, c->local_port); vga_print(detail);
    
    if (strlen(c->remote_addr) > 0) {
        sprintf(detail, "Remote Address:   %s:%d\n", c->remote_addr, c->remote_port);
    } else {
        sprintf(detail, "Remote Address:   (Listening)\n");
    }
    vga_print(detail);
    
    sprintf(detail, "State:            %s\n", c->state); vga_print(detail);
    sprintf(detail, "PID:              %d\n", c->pid); vga_print(detail);
    sprintf(detail, "Program:          %s\n", c->program); vga_print(detail);
    
    char sent_str[16], recv_str[16];
    format_bytes(c->bytes_sent, sent_str, sizeof(sent_str));
    format_bytes(c->bytes_recv, recv_str, sizeof(recv_str));
    
    sprintf(detail, "Bytes Sent:       %s\n", sent_str); vga_print(detail);
    sprintf(detail, "Bytes Received:   %s\n", recv_str); vga_print(detail);
}

void network_monitor_run(void) {
    srand(time(NULL));
    generate_connections();
    
    int show_detail = 0;
    int running = 1;
    
    while (running) {
        draw_header();
        draw_connections();
        draw_statistics();
        
        if (show_detail) {
            draw_connection_detail(selected_conn);
        }
        
        if (kbhit()) {
            char key;
            vga_getc(&key);
            
            switch (key) {
                case 72: case 'w': case 'W':
                    if (selected_conn > 0) selected_conn--;
                    break;
                    
                case 80: case 's': case 'S':
                    if (selected_conn < connection_count - 1) selected_conn++;
                    break;
                    
                case 'd': case 'D':
                    show_detail = !show_detail;
                    break;
                    
                case 'r': case 'R':
                    generate_connections();
                    vga_setcolor(0x0A, 0);
                    vga_print("\n[Network data refreshed]\n");
                    break;
                    
                case 'q': case 'Q':
                    running = 0;
                    break;
            }
        }
    }
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux Network Monitor...\n");
    network_monitor_run();
    return 0;
}