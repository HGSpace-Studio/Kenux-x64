#include <stddef.h>
#include "efi.h"

// 简单的 UEFI 应用程序
efi_status_t efi_main(efi_handle_t image_handle, efi_system_table_t *system_table) {
    (void)image_handle;
    
    // 尝试获取文本输出协议
    efi_simple_text_output_t *con_out = system_table->con_out;
    if (!con_out) {
        while (1);
    }
    
    // 输出字符串
    char16_t msg[] = u"KEGD Bootloader Started!\r\n";
    con_out->output_string(con_out, msg);
    
    // 无限循环
    while (1) {
        __asm__ volatile ("hlt");
    }
    
    return EFI_SUCCESS;
}
