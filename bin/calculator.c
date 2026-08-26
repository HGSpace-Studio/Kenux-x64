#include <stdio.h>
#include <string.h>
#include <vga.h>

void calculator_run(void)
{
    vga_print("=== Kenux Calculator ===\n");
    vga_print("Enter expressions (e.g., 2+3, 5*6, 10/2)\n");
    vga_print("Operations: + - * /\n");
    vga_print("Type 'exit' to quit\n\n");
    
    while (1) {
        char line[128];
        vga_print("calc> ");
        vga_gets(line, sizeof(line));
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        if (strlen(line) == 0) {
            continue;
        }
        
        int a = 0, b = 0;
        char op = '+';
        int i = 0;
        int negative = 0;
        
        if (line[0] == '-') {
            negative = 1;
            i = 1;
        }
        
        while (line[i] && line[i] >= '0' && line[i] <= '9') {
            a = a * 10 + (line[i] - '0');
            i++;
        }
        
        if (negative) {
            a = -a;
        }
        
        if (line[i]) {
            op = line[i];
            i++;
        }
        
        negative = 0;
        if (line[i] == '-') {
            negative = 1;
            i++;
        }
        
        while (line[i] && line[i] >= '0' && line[i] <= '9') {
            b = b * 10 + (line[i] - '0');
            i++;
        }
        
        if (negative) {
            b = -b;
        }
        
        double result = 0;
        if (op == '+') {
            result = a + b;
        } else if (op == '-') {
            result = a - b;
        } else if (op == '*') {
            result = a * b;
        } else if (op == '/' && b != 0) {
            result = (double)a / (double)b;
        } else if (b == 0) {
            vga_print("Error: Division by zero\n");
            continue;
        } else {
            vga_print("Error: Unknown operator\n");
            continue;
        }
        
        vga_print("Result: ");
        if (result == (int)result) {
            char buf[32];
            sprintf(buf, "%d\n", (int)result);
            vga_print(buf);
        } else {
            char buf[32];
            sprintf(buf, "%.2f\n", result);
            vga_print(buf);
        }
    }
    
    vga_print("Calculator exited\n");
}

int main(int argc, char** argv)
{
    calculator_run();
    return 0;
}
