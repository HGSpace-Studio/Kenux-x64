# Makefile for Kenux Kernel Enhanced Features
# SPDX-License-Identifier: MIT
#
# This Makefile builds the new enhanced features for Kenux Kernel
# including extended VFS, device management, memory management,
# logging, and tracing systems.

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -O2
INCLUDE := -I include -I kernel/include -I kernel/arch/x86_64/include -I kernel/arch/x86_64/include/arch -I kernel/lib/libc/include

# Feature modules to build
FEATURE_MODULES := \
    vfs_ext \
    device_manager \
    memory_ext \
    logging \
    trace \
    crypto_fs \
    lvm \
    raid \
    wifi \
    vpn \
    ipv6 \
    power_mgmt \
    security \
    debug_tools \
    virtualization

# Source files for each feature
VFS_EXT_SRC := \
    kernel/api/kapi_vfs_ext.c

DEVICE_MANAGER_SRC := \
    kernel/api/kapi_device_manager.c

MEMORY_EXT_SRC := \
    kernel/api/kapi_memory_ext.c

LOGGING_SRC := \
    kernel/api/kapi_logging.c

TRACE_SRC := \
    kernel/api/kapi_trace.c

CRYPTO_FS_SRC := \
    kernel/api/kapi_crypto_fs.c

LVM_SRC := \
    kernel/api/kapi_lvm.c

RAID_SRC := \
    kernel/api/kapi_raid.c

WIFI_SRC := \
    kernel/api/kapi_wifi.c

VPN_SRC := \
    kernel/api/kapi_vpn.c

IPV6_SRC := \
    kernel/net/ipv6/ipv6.c \
    kernel/net/ipv6/icmpv6.c \
    kernel/net/ipv6/ndp.c

POWER_MGMT_SRC := \
    kernel/power/acpi.c \
    kernel/power/cpufreq.c \
    kernel/power/pm.c

SECURITY_SRC := \
    kernel/security/selinux.c \
    kernel/security/capabilities.c \
    kernel/security/apparmor.c

DEBUG_TOOLS_SRC := \
    kernel/debug/gdb_stub.c \
    kernel/debug/profiler.c \
    kernel/debug/memleak.c

VIRTUALIZATION_SRC := \
    kernel/virt/kvm.c \
    kernel/virt/container.c \
    kernel/virt/cgroup.c

# Generate object files
OBJ := $(addsuffix .o,$(FEATURE_MODULES))

# Source file to object file mapping
$(VFS_EXT_SRC:.c=.o): CFLAGS += -D__VFS_EXT__
$(DEVICE_MANAGER_SRC:.c=.o): CFLAGS += -D__DEVICE_MANAGER__
$(MEMORY_EXT_SRC:.c=.o): CFLAGS += -D__MEMORY_EXT__
$(LOGGING_SRC:.c=.o): CFLAGS += -D__LOGGING__
$(TRACE_SRC:.c=.o): CFLAGS += -D__TRACE__
$(CRYPTO_FS_SRC:.c=.o): CFLAGS += -D__CRYPTO_FS__
$(LVM_SRC:.c=.o): CFLAGS += -D__LVM__
$(RAID_SRC:.c=.o): CFLAGS += -D__RAID__
$(WIFI_SRC:.c=.o): CFLAGS += -D__WIFI__
$(VPN_SRC:.c=.o): CFLAGS += -D__VPN__
$(IPV6_SRC:.c=.o): CFLAGS += -D__IPV6__
$(POWER_MGMT_SRC:.c=.o): CFLAGS += -D__POWER_MGMT__
$(SECURITY_SRC:.c=.o): CFLAGS += -D__SECURITY__
$(DEBUG_TOOLS_SRC:.c=.o): CFLAGS += -D__DEBUG_TOOLS__
$(VIRTUALIZATION_SRC:.c=.o): CFLAGS += -D__VIRTUALIZATION__

# Rule to build object files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Main build target: Enhanced kernel module
enhanced_kernel: $(OBJ)
	@echo "Linking enhanced kernel module..."
	ld -r -o kernel_enhanced.o $(OBJ)

# Build individual features
vfs_ext: $(VFS_EXT_SRC:.c=.o)
device_manager: $(DEVICE_MANAGER_SRC:.c=.o)
memory_ext: $(MEMORY_EXT_SRC:.c=.o)
logging: $(LOGGING_SRC:.c=.o)
trace: $(TRACE_SRC:.c=.o)
crypto_fs: $(CRYPTO_FS_SRC:.c=.o)
lvm: $(LVM_SRC:.c=.o)
raid: $(RAID_SRC:.c=.o)
wifi: $(WIFI_SRC:.c=.o)
vpn: $(VPN_SRC:.c=.o)
ipv6: $(IPV6_SRC:.c=.o)
power_mgmt: $(POWER_MGMT_SRC:.c=.o)
security: $(SECURITY_SRC:.c=.o)
debug_tools: $(DEBUG_TOOLS_SRC:.c=.o)
virtualization: $(VIRTUALIZATION_SRC:.c=.o)

# Build all features
all: enhanced_kernel

# Test targets
test_logging: $(LOGGING_SRC:.c=.o)
	@echo "Testing logging system..."
	$(CC) $(CFLAGS) $(INCLUDE) -DTEST_LOGGING $(LOGGING_SRC:.c=.o) -o test_logging
	./test_logging

test_memory: $(MEMORY_EXT_SRC:.c=.o)
	@echo "Testing memory system..."
	$(CC) $(CFLAGS) $(INCLUDE) -DTEST_MEMORY $(MEMORY_EXT_SRC:.c=.o) -o test_memory
	./test_memory

test_vfs: $(VFS_EXT_SRC:.c=.o)
	@echo "Testing VFS extensions..."
	$(CC) $(CFLAGS) $(INCLUDE) -DTEST_VFS $(VFS_EXT_SRC:.c=.o) -o test_vfs
	./test_vfs

# Installation targets
install: enhanced_kernel
	@echo "Installing enhanced kernel module..."
	# Installation would be implemented here
	# For now, just copy to appropriate location

install_headers:
	@echo "Installing header files..."
	# Copy all header files to system include directory

# Clean targets
clean:
	@echo "Cleaning object files..."
	rm -f $(OBJ) kernel_enhanced.o test_logging test_memory test_vfs

distclean: clean
	@echo "Cleaning all generated files..."
	rm -f *.a *.so

.PHONY: all clean distclean install install_headers \
        vfs_ext device_manager memory_ext logging trace \
        crypto_fs lvm raid wifi vpn ipv6 power_mgmt \
        security debug_tools virtualization \
        test_logging test_memory test_vfs