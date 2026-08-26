# SPDX-License-Identifier: MIT
# CMakeLists.txt - KAL 内核抽象层构建脚本
cmake_minimum_required(VERSION 3.13)
project(kal C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 让 MSVC 也吃 _CRT_SECURE_NO_WARNINGS,避免 strcpy 之类告警
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- KAL 静态库 ----
add_library(kal STATIC
    src/kal_errno.c
    src/kal_capability.c
    src/kal_marshalling.c
    src/kal_registry.c
    src/kal_dispatcher.c
    src/kal_governance.c
    src/kal_ssi.c
    adapters/linuxlike_adapter.c
    adapters/custom_adapter.c
    adapters/microkernel_adapter.c
    adapters/kal_builtins.c
)
target_include_directories(kal PUBLIC include)

# Windows 平台需要链接 QueryPerformanceFrequency 所在的 kernel32
if(WIN32)
    target_link_libraries(kal PUBLIC kernel32)
endif()

# ---- demo 可执行 ----
add_executable(kal_demo examples/demo.c)
target_link_libraries(kal_demo PRIVATE kal)
