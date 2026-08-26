#include <elf.h>
#include <arch/elf.h>
#include <arch/memory.h>
#include <arch/process.h>
#include <arch/fs.h>
#include <string.h>
#include <slab.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>

// ELF加载增强功能
typedef struct {
    elf_load_info_t base;
    uint64_t loaded_base;
    uint64_t entry_point;
    uint64_t brk_base;
    uint64_t brk_size;
    uint32_t load_flags;
} enhanced_elf_info_t;

// ELF加载标志
#define ELF_LOAD_HEAP_ENABLED    (1 << 0)
#define ELF_LOAD_BSS_INIT        (1 << 1)
#define ELF_LOAD_RELRO          (1 << 2)

// 初始化ELF加载器增强功能
int elf_loader_init(void) {
    // 初始化必要的子系统
    memory_init_allocator();
    security_init();
    return 0;
}

// 加载ELF文件的辅助函数
int elf_load_segments(const char* path, enhanced_elf_info_t* info) {
    if (!path || !info) return -1;
    
    int fd = fs_open_file(path, "r");
    if (fd < 0) return -1;
    
    // 读取ELF头部
    Elf64_Ehdr ehdr;
    if (fs_read_file_data(fd, &ehdr, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr)) {
        fs_close_file(fd);
        return -1;
    }
    
    // 验证ELF文件
    if (elf_verify(&ehdr) != 0) {
        fs_close_file(fd);
        return -1;
    }
    
    // 读取程序头表
    Elf64_Phdr* phdrs = kzalloc(ehdr.e_phnum * sizeof(Elf64_Phdr));
    if (!phdrs) {
        fs_close_file(fd);
        return -1;
    }
    
    // 定位到程序头表
    uint64_t phdr_offset = ehdr.e_phoff;
    fs_seek(fd, phdr_offset, SEEK_SET);
    
    // 读取程序头表
    if (fs_read_file_data(fd, phdrs, ehdr.e_phnum * sizeof(Elf64_Phdr)) != ehdr.e_phnum * sizeof(Elf64_Phdr)) {
        kfree(phdrs);
        fs_close_file(fd);
        return -1;
    }
    
    // 加载段
    uint64_t min_vaddr = ~0ULL;
    uint64_t max_vaddr = 0;
    
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr* ph = &phdrs[i];
        
        if (ph->p_type == PT_LOAD) {
            // 更新地址范围
            if (ph->p_vaddr < min_vaddr) min_vaddr = ph->p_vaddr;
            uint64_t end = ph->p_vaddr + ph->p_memsz;
            if (end > max_vaddr) max_vaddr = end;
            
            // 分配内存
            uint64_t aligned_vaddr = ph->p_vaddr & ~(PAGE_SIZE - 1);
            uint64_t aligned_size = (ph->p_filesz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            
            uint64_t paddr = memory_alloc_physical(aligned_size / PAGE_SIZE);
            if (!paddr) {
                kfree(phdrs);
                fs_close_file(fd);
                return -1;
            }
            
            // 映射段
            for (uint64_t offset = 0; offset < aligned_size; offset += PAGE_SIZE) {
                uint64_t vaddr = aligned_vaddr + offset;
                uint64_t flags = PAGE_PRESENT | PAGE_USER;
                
                if (ph->p_flags & PF_W) flags |= PAGE_WRITABLE;
                if (ph->p_flags & PF_X) flags |= PAGE_EXECUTE;
                
                memory_map_user(vaddr, paddr + offset, flags);
            }
            
            // 加载段数据
            if (ph->p_filesz > 0) {
                fs_seek(fd, ph->p_offset, SEEK_SET);
                char* buf = kzalloc(ph->p_filesz);
                if (!buf) {
                    kfree(phdrs);
                    fs_close_file(fd);
                    return -1;
                }
                
                if (fs_read_file_data(fd, buf, ph->p_filesz) == ph->p_filesz) {
                    uint64_t dest = ph->p_vaddr;
                    memcpy((void*)dest, buf, ph->p_filesz);
                }
                
                kfree(buf);
            }
            
            // 初始化BSS
            if (ph->p_memsz > ph->p_filesz) {
                uint64_t bss_start = ph->p_vaddr + ph->p_filesz;
                uint64_t bss_size = ph->p_memsz - ph->p_filesz;
                memset((void*)bss_start, 0, bss_size);
            }
        }
    }
    
    // 设置堆
    info->brk_base = max_vaddr;
    info->brk_size = 4 * 1024 * 1024; // 4MB默认堆大小
    
    kfree(phdrs);
    fs_close_file(fd);
    
    return 0;
}

// 设置ELF程序入口点
int elf_setup_entry_point(enhanced_elf_info_t* info, const char* argv[], const char* envp[]) {
    if (!info) return -1;
    
    // 准备栈
    uint64_t stack_top = USER_STACK_TOP;
    uint64_t* sp = (uint64_t*)stack_top;
    
    // 计算参数和环境的数量
    int argc = 0;
    int envc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }
    if (envp) {
        while (envp[envc]) envc++;
    }
    
    // 保留16字节的对齐空间
    sp -= 2;
    
    // 传递环境指针
    if (envp) {
        for (int i = 0; i < envc; i++) {
            *(--sp) = (uint64_t)envp[i];
        }
    }
    *(--sp) = 0; // NULL终止的环境指针
    
    // 传递参数指针
    if (argv) {
        for (int i = argc - 1; i >= 0; i--) {
            *(--sp) = (uint64_t)argv[i];
        }
    }
    *(--sp) = 0; // NULL终止的参数指针
    
    // 设置argc
    *(--sp) = argc;
    
    // 保留8字节对齐
    sp = (uint64_t*)((uintptr_t)sp & ~0x7);
    
    // 设置栈指针
    info->stack_top = (uint64_t)sp;
    
    return 0;
}

// 运行ELF程序
int elf_run_program(const char* path, const char* argv[], const char* envp[]) {
    if (!path) return -1;
    
    enhanced_elf_info_t info;
    memset(&info, 0, sizeof(info));
    
    // 加载ELF文件
    if (elf_load_segments(path, &info) != 0) {
        return -1;
    }
    
    // 设置入口点
    if (elf_setup_entry_point(&info, argv, envp) != 0) {
        return -1;
    }
    
    // 创建新进程
    process_t* proc = process_create(path);
    if (!proc) {
        return -1;
    }
    
    // 设置进程的ELF信息
    proc->entry_point = info.base.entry;
    proc->stack_top = info.stack_top;
    
    // 启动进程
    int pid = process_start(proc);
    if (pid < 0) {
        process_destroy(proc);
        return -1;
    }
    
    return pid;
}

// 检查ELF文件兼容性
int elf_check_compatibility(const char* path) {
    if (!path) return -1;
    
    int fd = fs_open_file(path, "r");
    if (fd < 0) return -1;
    
    Elf64_Ehdr ehdr;
    if (fs_read_file_data(fd, &ehdr, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr)) {
        fs_close_file(fd);
        return -1;
    }
    
    // 检查基本的兼容性要求
    if (ehdr.e_ident[0] != ELFMAG0 ||
        ehdr.e_ident[1] != ELFMAG1 ||
        ehdr.e_ident[2] != ELFMAG2 ||
        ehdr.e_ident[3] != ELFMAG3) {
        fs_close_file(fd);
        return -1;
    }
    
    if (ehdr.e_ident[4] != ELFCLASS64) {
        fs_close_file(fd);
        return -1;
    }
    
    if (ehdr.e_ident[5] != ELFDATA2LSB) {
        fs_close_file(fd);
        return -1;
    }
    
    if (ehdr.e_machine != EM_X86_64) {
        fs_close_file(fd);
        return -1;
    }
    
    // 检查是否支持动态链接
    if (ehdr.e_type == ET_DYN) {
        // 需要检查解释器支持
        // TODO: 实现动态链接器
    }
    
    fs_close_file(fd);
    return 0;
}

// 获取ELF文件信息
int elf_get_file_info(const char* path, enhanced_elf_info_t* info) {
    if (!path || !info) return -1;
    
    int fd = fs_open_file(path, "r");
    if (fd < 0) return -1;
    
    Elf64_Ehdr ehdr;
    if (fs_read_file_data(fd, &ehdr, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr)) {
        fs_close_file(fd);
        return -1;
    }
    
    if (elf_verify(&ehdr) != 0) {
        fs_close_file(fd);
        return -1;
    }
    
    // 填充基本信息
    info->base.entry = ehdr.e_entry;
    info->base.phnum = ehdr.e_phnum;
    info->base.phentsize = ehdr.e_phentsize;
    info->base.is_dynamic = (ehdr.e_type == ET_DYN);
    
    fs_close_file(fd);
    return 0;
}