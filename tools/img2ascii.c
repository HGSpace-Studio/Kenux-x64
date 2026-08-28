#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ASCII_CHARS " .:-=+*#%@"
#define ASCII_CHARS_LEN 10

typedef struct {
    int width;
    int height;
    unsigned char* data;
    int channels;
} Image;

static Image load_image(const char* filename) {
    Image img = {0, 0, NULL, 0};
    
    FILE* f = fopen(filename, "rb");
    if (!f) return img;
    
    char header[54];
    if (fread(header, 1, 54, f) != 54) {
        fclose(f);
        return img;
    }
    
    if (header[0] != 'B' || header[1] != 'M') {
        fclose(f);
        printf("Error: Not a BMP file\n");
        return img;
    }
    
    int data_offset = *(int*)&header[10];
    img.width = *(int*)&header[18];
    img.height = abs(*(int*)&header[22]);
    int bits_per_pixel = *(short int*)&header[28];
    img.channels = bits_per_pixel / 8;
    
    fseek(f, data_offset, SEEK_SET);
    
    int row_size = (img.width * img.channels + 3) & ~3;
    int data_size = row_size * img.height;
    
    img.data = (unsigned char*)malloc(data_size);
    if (!img.data) {
        fclose(f);
        return img;
    }
    
    fread(img.data, 1, data_size, f);
    fclose(f);
    
    return img;
}

static void free_image(Image* img) {
    if (img->data) {
        free(img->data);
        img->data = NULL;
    }
}

static unsigned char get_pixel_gray(Image* img, int x, int y) {
    if (x < 0 || x >= img->width || y < 0 || y >= img->height) return 0;
    
    int row_size = (img->width * img->channels + 3) & ~3;
    int idx = y * row_size + x * img->channels;
    
    if (img->channels >= 3) {
        unsigned char r = img->data[idx + 2];
        unsigned char g = img->data[idx + 1];
        unsigned char b = img->data[idx];
        return (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
    } else {
        return img->data[idx];
    }
}

static char gray_to_ascii(unsigned char gray) {
    float ratio = gray / 255.0f;
    int idx = (int)(ratio * (ASCII_CHARS_LEN - 1));
    if (idx < 0) idx = 0;
    if (idx >= ASCII_CHARS_LEN) idx = ASCII_CHARS_LEN - 1;
    return ASCII_CHARS[ASCII_CHARS_LEN - 1 - idx];
}

static void convert_to_ascii(Image* img, int output_width, int use_color, const char* output_file) {
    if (output_width <= 0) output_width = 80;
    
    double aspect_ratio = 2.0;
    int output_height = (int)(img->height * ((double)output_width / img->width) / aspect_ratio);
    
    FILE* out = stdout;
    if (output_file && strlen(output_file) > 0) {
        out = fopen(output_file, "w");
        if (!out) {
            printf("Error: Cannot open output file: %s\n", output_file);
            return;
        }
    }
    
    for (int y = 0; y < output_height; y++) {
        for (int x = 0; x < output_width; x++) {
            int src_x = (int)((double)x / output_width * img->width);
            int src_y = (int)((double)y / output_height * img->height);
            
            unsigned char gray = get_pixel_gray(img, src_x, src_y);
            
            if (use_color && img->channels >= 3) {
                int row_size = (img->width * img->channels + 3) & ~3;
                int idx = src_y * row_size + src_x * img->channels;
                unsigned char r = img->data[idx + 2];
                unsigned char g = img->data[idx + 1];
                unsigned char b = img->data[idx];
                
                fprintf(out, "\033[48;2;%d;%d;%dm%c\033[0m", r, g, b, gray_to_ascii(gray));
            } else {
                fputc(gray_to_ascii(gray), out);
            }
        }
        fputc('\n', out);
    }
    
    if (out != stdout) {
        fclose(out);
        printf("Output saved to: %s\n", output_file);
    }
}

static void print_usage(const char* prog) {
    printf("img2ascii - Convert images to ASCII art\n\n");
    printf("Usage: %s [options] <image.bmp>\n\n", prog);
    printf("Options:\n");
    printf("  -w <width>     Output width in characters (default: 80)\n");
    printf("  -c             Enable color output (ANSI 256-color)\n");
    printf("  -o <file>      Output to file instead of stdout\n");
    printf("  -h             Show this help message\n\n");
    printf("Supported formats: BMP (24-bit and 8-bit)\n");
    printf("\nExample:\n");
    printf("  %s -w 100 -c logo.bmp > logo.txt\n", prog);
    printf("  %s -o ascii_art.png image.bmp\n", prog);
}

static void generate_kenux_logo_ascii(void) {
    const char* kenux_logo[] = {
        "        .--.   .--.   .-..      ",
        "       : .--\\ : .--\\ : :      ",
        "       : :   : :   : : :      ",
        "       : :   : :   : : :      ",
        "       : :   : :   : : :      ",
        "       : '--.' : '--.' : :     ",
        "       '--'   '--'   '--'      ",
        "",
        "   K E N U X   O S          ",
        "                            ",
        "  ██╗   ██╗██╗   ██╗███╗   ███╗",
        "  ██║   ██║██║   ██║████╗ ████║",
        "  ██║   ██║██║   ██║██╔████╔██║",
        "  ╚██╗ ██╔╝██║   ██║██║╚██╔╝██║",
        "   ╚████╔╝ ╚██████╔╝██║ ╚═╝ ██║",
        "    ╚═══╝   ╚═════╝ ╚═╝     ╚═╝",
        "",
        "  Kernel 26.7.9K              "
    };
    
    printf("\033[1;36m");
    for (int i = 0; i < sizeof(kenux_logo) / sizeof(kenux_logo[0]); i++) {
        printf("%s\n", kenux_logo[i]);
    }
    printf("\033[0m\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        printf("\nGenerating Kenux default logo...\n\n");
        generate_kenux_logo_ascii();
        return 0;
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (strcmp(argv[1], "--logo") == 0) {
        generate_kenux_logo_ascii();
        return 0;
    }
    
    const char* input_file = NULL;
    int output_width = 80;
    int use_color = 0;
    const char* output_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            output_width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0) {
            use_color = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        }
    }
    
    if (!input_file) {
        printf("Error: No input file specified\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    Image img = load_image(input_file);
    if (!img.data) {
        printf("Error: Failed to load image: %s\n", input_file);
        return 1;
    }
    
    printf("Loaded image: %dx%d, %d channels\n", img.width, img.height, img.channels);
    
    convert_to_ascii(&img, output_width, use_color, output_file);
    
    free_image(&img);
    
    return 0;
}