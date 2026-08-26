# SPDX-License-Identifier: MIT
# Makefile - KAL 构建与运行(mingw32-make / make 均可)
#
#   mingw32-make            # 构建静态库 + demo
#   mingw32-make run        # 构建并运行 demo
#   mingw32-make clean
#
# 需要把 MinGW 的 bin 放进 PATH,或设置 CC=.../gcc.exe

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast
INCLUDE := -I include
LDLIBS  := -lkernel32

SRC := \
    src/kal_errno.c \
    src/kal_capability.c \
    src/kal_marshalling.c \
    src/kal_registry.c \
    src/kal_dispatcher.c \
    src/kal_governance.c \
    src/kal_ssi.c \
    adapters/linuxlike_adapter.c \
    adapters/custom_adapter.c \
    adapters/microkernel_adapter.c \
    adapters/kal_builtins.c

DEMO_SRC := examples/demo.c

.PHONY: all run clean
all: kal_demo.exe

kal_demo.exe: $(SRC) $(DEMO_SRC)
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) $(DEMO_SRC) -o $@ $(LDLIBS)

run: kal_demo.exe
	./kal_demo.exe

clean:
	rm -f kal_demo.exe
