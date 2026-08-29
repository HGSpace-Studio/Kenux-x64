# Integrated Build Configuration for Kenux Kernel
# SPDX-License-Identifier: MIT
#
# This Makefile integrates all Kenux Kernel components with new enhanced features

# Compiler and flags
CC ?= gcc
LD ?= ld
AR ?= ar

# Base compiler flags
CFLAGS_BASE := -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -O2 -g
CFLAGS_KERNEL := -ffreestanding -fno-pic -fno-pie -nostdlib -nostartfiles -nodefaultlibs
CFLAGS_KERNEL_ARCH := -m64 -mcmodel=kernel

# Include paths
INCLUDE_BASE := -I include -I kernel/include -I kernel/arch/x86_64/include -I kernel/arch/x86_64/include/arch -I kernel/lib/libc/include
INCLUDE_ENHANCED := -I enhanced_features/include -I enhanced_features/kernel/include -I enhanced_features/kernel/arch/x86_64/include

# Linker flags
LDFLAGS_KERNEL := -T kernel/arch/x86_64/boot/linker.ld -no-pie -nostdlib -nostartfiles -nodefaultlibs

# Build directories
BUILD_DIR := build
BUILD_KERNEL_DIR := $(BUILD_DIR)/kernel
BUILD_ENHANCED_DIR := $(BUILD_DIR)/enhanced_features
BUILD_APPS_DIR := $(BUILD_DIR)/apps

# Kernel source files
KERNEL_SRC := \
    kernel/api/*.c \
    kernel/arch/x86_64/*.c \
    kernel/arch/x86_64/boot/*.c \
    kernel/lib/libc/*.c

# Enhanced features source files
ENHANCED_FEATURES_SRC := \
    enhanced_features/kernel/api/kapi_vfs_ext.c \
    enhanced_features/kernel/api/kapi_device_manager.c \
    enhanced_features/kernel/api/kapi_memory_ext.c \
    enhanced_features/kernel/api/kapi_logging.c \
    enhanced_features/kernel/api/kapi_trace.c \
    enhanced_features/kernel/api/kapi_crypto_fs.c \
    enhanced_features/kernel/api/kapi_lvm.c \
    enhanced_features/kernel/api/kapi_raid.c \
    enhanced_features/kernel/api/kapi_wifi.c \
    enhanced_features/kernel/api/kapi_vpn.c \
    enhanced_features/kernel/api/kapi_power_mgmt.c \
    enhanced_features/kernel/api/kapi_security.c \
    enhanced_features/kernel/api/kapi_debug_tools.c \
    enhanced_features/kernel/api/kapi_virtualization.c

# Application source files
APPS_SRC := \
    apps/*.c

# Object files
KERNEL_OBJ := $(patsubst %.c,$(BUILD_KERNEL_DIR)/%.o,$(notdir $(KERNEL_SRC)))
ENHANCED_OBJ := $(patsubst %.c,$(BUILD_ENHANCED_DIR)/%.o,$(notdir $(ENHANCED_FEATURES_SRC)))
APPS_OBJ := $(patsubst %.c,$(BUILD_APPS_DIR)/%.o,$(notdir $(APPS_SRC)))

# Main kernel target
kernel:
	python3 build.py run kernel

# Enhanced features target
enhanced_features:
	python3 build.py run components

# Applications target
apps:
	python3 build.py run components

# Complete system target
all:
	python3 build.py run all

# Build kernel object files
$(BUILD_KERNEL_DIR)/%.o: kernel/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS_BASE) $(CFLAGS_KERNEL) $(CFLAGS_KERNEL_ARCH) $(INCLUDE_BASE) -c $< -o $@

# Build enhanced features object files
$(BUILD_ENHANCED_DIR)/%.o: enhanced_features/kernel/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS_BASE) $(CFLAGS_KERNEL) $(CFLAGS_KERNEL_ARCH) $(INCLUDE_BASE) $(INCLUDE_ENHANCED) -c $< -o $@

# Build applications object files
$(BUILD_APPS_DIR)/%.o: apps/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS_BASE) $(INCLUDE_BASE) -c $< -o $@

# Build directories
$(BUILD_KERNEL_DIR) $(BUILD_ENHANCED_DIR) $(BUILD_APPS_DIR):
	@echo "Creating build directory: $@"
	mkdir -p $@

# Clean targets
clean:
	python3 build.py run clean

distclean: clean
	@echo "Cleaning all generated files..."
	rm -f *.o *.a *.elf *.exe

# Install targets
install: kernel enhanced_features apps
	@echo "Installing Kenux Kernel..."
	# Installation would be implemented here
	# Copy kernel.elf to /boot/
	# Copy enhanced_features.a to /lib/
	# Copy kernel_apps to /bin/

install_headers:
	@echo "Installing header files..."
	# Copy all header files to system include directory
	cp -r include /usr/include/kenux_kernel
	cp -r enhanced_features/include /usr/include/kenux_enhanced

# Test targets
test_kernel: kernel
	@echo "Running kernel tests..."
	# Kernel tests would be implemented here

test_enhanced: enhanced_features
	@echo "Running enhanced features tests..."
	# Enhanced features tests would be implemented here

test_apps: apps
	@echo "Running application tests..."
	# Application tests would be implemented here

# Integration test targets
integration_test: all
	@echo "Running integration tests..."
	# Integration tests would be implemented here

# Documentation targets
docs:
	@echo "Generating documentation..."
	# Documentation generation would be implemented here

# Development targets
debug: CFLAGS_BASE += -g -DDEBUG
debug: all

release: CFLAGS_BASE += -O3 -DNDEBUG
release: all

.PHONY: all kernel enhanced_features apps clean distclean \
        install install_headers test_kernel test_enhanced test_apps \
        integration_test docs debug release
