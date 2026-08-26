#include <stdio.h>
#include <string.h>
#include <vga.h>

static const char* months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int get_day_of_week(int year, int month, int day)
{
    if (month < 3) {
        month += 12;
        year--;
    }
    
    int k = year % 100;
    int j = year / 100;
    int day_of_week = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    
    return (day_of_week + 6) % 7;
}

void print_month(int year, int month)
{
    vga_print(months[month - 1]);
    vga_print(" ");
    char buf[16];
    sprintf(buf, "%d\n", year);
    vga_print(buf);
    
    vga_print("Su Mo Tu We Th Fr Sa\n");
    
    int first_day = get_day_of_week(year, month, 1);
    
    for (int i = 0; i < first_day; i++) {
        vga_print("   ");
    }
    
    int num_days = days_in_month[month - 1];
    if (month == 2 && is_leap_year(year)) {
        num_days = 29;
    }
    
    for (int day = 1; day <= num_days; day++) {
        char buf[8];
        sprintf(buf, "%2d ", day);
        vga_print(buf);
        
        if ((first_day + day) % 7 == 0) {
            vga_print("\n");
        }
    }
    
    vga_print("\n");
}

void calendar_run(void)
{
    vga_print("=== Kenux Calendar ===\n");
    vga_print("Usage: calendar [month] [year]\n");
    vga_print("       calendar year [year]\n\n");
    
    int current_year = 2026;
    int current_month = 3;
    
    if (argc >= 2) {
        current_month = atoi(argv[1]);
        if (argc >= 3) {
            current_year = atoi(argv[2]);
        }
    }
    
    if (current_month == 0 && argc >= 2 && strcmp(argv[1], "year") == 0) {
        if (argc >= 3) {
            current_year = atoi(argv[2]);
        }
        
        vga_print("Calendar for ");
        char buf[16];
        sprintf(buf, "%d\n\n", current_year);
        vga_print(buf);
        
        for (int m = 1; m <= 12; m++) {
            print_month(current_year, m);
            vga_print("\n");
        }
    } else {
        if (current_month < 1 || current_month > 12) {
            vga_print("Invalid month. Using current month (3).\n");
            current_month = 3;
        }
        
        print_month(current_year, current_month);
    }
}

int main(int argc, char** argv)
{
    calendar_run();
    return 0;
}
