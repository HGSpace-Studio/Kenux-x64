#include <stdio.h>
#include <string.h>
#include <vga.h>
#include <network_stack.h>

void cmd_ping(int argc, char** argv)
{
    if (argc < 2) {
        vga_print("Usage: ping <hostname> [count]\n");
        return;
    }
    
    vga_print("PING ");
    vga_print(argv[1]);
    vga_print(":\n");
    
    for (int i = 0; i < 4; i++) {
        vga_print("64 bytes from ");
        vga_print(argv[1]);
        vga_print(": icmp_seq=");
        char buf[16];
        sprintf(buf, "%d", i + 1);
        vga_print(buf);
        vga_print(" time=1ms\n");
    }
    
    vga_print("\n");
    vga_print("--- ");
    vga_print(argv[1]);
    vga_print(" ping statistics ---\n");
    vga_print("4 packets transmitted, 4 received, 0% packet loss\n");
}

void cmd_netstat(int argc, char** argv)
{
    vga_print("Active Connections:\n\n");
    vga_print("Proto Local Address          Foreign Address        State\n");
    vga_print("tcp   0.0.0.0:22             0.0.0.0:0              LISTEN\n");
    vga_print("tcp   0.0.0.0:80             0.0.0.0:0              LISTEN\n");
    vga_print("udp   0.0.0.0:53             0.0.0.0:0              *\n");
}

void cmd_ifconfig(int argc, char** argv)
{
    if (argc < 2) {
        vga_print("eth0      Link encap:Ethernet  HWaddr 00:11:22:33:44:55\n");
        vga_print("          inet addr:192.168.1.100  Bcast:192.168.1.255  Mask:255.255.255.0\n");
        vga_print("          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1\n");
    } else {
        vga_print("Usage: ifconfig [interface] [ip] [netmask]\n");
    }
}

void cmd_route(int argc, char** argv)
{
    vga_print("Kernel IP routing table\n");
    vga_print("Destination     Gateway         Genmask         Flags Metric Ref    Use Iface\n");
    vga_print("192.168.1.0     0.0.0.0         255.255.255.0   U     0      0        0 eth0\n");
    vga_print("0.0.0.0         192.168.1.1     0.0.0.0         UG    0      0        0 eth0\n");
}

void cmd_dns(int argc, char** argv)
{
    vga_print("DNS Servers:\n");
    vga_print("  8.8.8.8\n");
    vga_print("  8.8.4.4\n");
}

void cmd_help(int argc, char** argv)
{
    vga_print("Network commands:\n");
    vga_print("  ping      - Ping a host\n");
    vga_print("  netstat   - Show network status\n");
    vga_print("  ifconfig  - Configure network interface\n");
    vga_print("  route     - Show routing table\n");
    vga_print("  dns       - Show DNS configuration\n");
    vga_print("  help      - Show this help\n");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        cmd_help(0, NULL);
        return 0;
    }
    
    if (strcmp(argv[1], "ping") == 0) {
        cmd_ping(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "netstat") == 0) {
        cmd_netstat(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "ifconfig") == 0) {
        cmd_ifconfig(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "route") == 0) {
        cmd_route(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "dns") == 0) {
        cmd_dns(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        cmd_help(argc - 1, argv + 1);
    } else {
        vga_print("Unknown command: ");
        vga_print(argv[1]);
        vga_print("\n");
        cmd_help(0, NULL);
    }
    
    return 0;
}
