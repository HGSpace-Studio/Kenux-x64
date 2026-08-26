#define MULTIBOOT2_HEADER_MAGIC  0xE85250D6
#define MULTIBOOT2_HEADER_ARCH   1
#define MULTIBOOT2_HEADER_LENGTH (sizeof(multiboot2_header_t))
#define MULTIBOOT2_HEADER_CHECKSUM (-(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_HEADER_ARCH + MULTIBOOT2_HEADER_LENGTH))

typedef struct multiboot2_header multiboot2_header_t;

struct multiboot2_header
{
    uint32_t magic;
    uint32_t architecture;
    uint32_t header_length;
    uint32_t checksum;
};
