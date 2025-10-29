#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "gpio_control.h"

// IMX6ULL GPIO register base addresses
#define GPIO1_BASE 0x0209C000
#define GPIO5_BASE 0x020AC000

// GPIO register offsets
#define GPIO_DR     0x00  // Data Register
#define GPIO_GDIR   0x04  // Direction Register

static void* gpio1_base = NULL;
static void* gpio5_base = NULL;

// Map GPIO registers to memory
static void* map_gpio_registers(unsigned long base_addr) 
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        return NULL;
    }
    // 0x1000 = 4KB -- whole page
    void* ptr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base_addr);
    close(fd);
    
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    
    return ptr;
}

// Initialize GPIO (map registers)
int gpio_init(void) 
{
    gpio1_base = map_gpio_registers(GPIO1_BASE);
    gpio5_base = map_gpio_registers(GPIO5_BASE);
    
    if (!gpio1_base || !gpio5_base) {
        printf("GPIO映射失败\n");
        return -1;
    }
    
    // Set GPIO direction to output
    // GPIO1_IO03 (LED)
    volatile unsigned int* gpio1_gdir = (unsigned int*)(gpio1_base + GPIO_GDIR);
    *gpio1_gdir |= (1 << 3);
    
    // GPIO5_IO01 (BEEP)
    volatile unsigned int* gpio5_gdir = (unsigned int*)(gpio5_base + GPIO_GDIR);
    *gpio5_gdir |= (1 << 1);
    
    printf("GPIO initialized (direct register access)\n");
    return 0;
}

// Control LED (GPIO1_IO03)
void led_control(int on) 
{
    if (!gpio1_base) return;
    
    volatile unsigned int* gpio1_dr = (unsigned int*)(gpio1_base + GPIO_DR);
    
    if (on)
        *gpio1_dr &= ~(1 << 3);  // low = on
    else
        *gpio1_dr |= (1 << 3);   // high = off
}

// Control BEEP (GPIO5_IO01)
void beep_control(int on) 
{
    if (!gpio5_base) return;
    
    volatile unsigned int* gpio5_dr = (unsigned int*)(gpio5_base + GPIO_DR);
    
    if (on)
        *gpio5_dr &= ~(1 << 1);  // low = on
    else
        *gpio5_dr |= (1 << 1);   // high = off
}

// Cleanup (unmap memory)
void gpio_cleanup() 
{
    if (gpio1_base) 
    {
        munmap(gpio1_base, 0x1000);
        gpio1_base = NULL;
    }
    if (gpio5_base) 
    {
        munmap(gpio5_base, 0x1000);
        gpio5_base = NULL;
    }
}