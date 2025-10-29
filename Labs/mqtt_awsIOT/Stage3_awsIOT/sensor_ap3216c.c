#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "sensor_ap3216c.h"

static int i2c_fd = -1;

// Write register
static int i2c_write_reg(unsigned char reg, unsigned char value) 
{
    unsigned char buf[2] = {reg, value};
    if (write(i2c_fd, buf, 2) != 2) {
        perror("i2c write");
        return -1;
    }
    return 0;
}

// Read register
static int i2c_read_reg(unsigned char reg, unsigned char *buf, int len) 
{
    // I2C read flow:
    // 1. write 1 byte to set register address
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("i2c write reg addr");
        return -1;
    }
    // 2. Then read data from that register
    if (read(i2c_fd, buf, len) != len) {
        perror("i2c read");
        return -1;
    }
    return 0;
}

// Initialize AP3216C
int ap3216c_init(void) 
{
    // 1. Open I2C device
    i2c_fd = open(I2C_DEVICE, O_RDWR);
    if (i2c_fd < 0) 
    {
        perror("open i2c device");
        return -1;
    }
    
    // 2. Set slave address
    if (ioctl(i2c_fd, I2C_SLAVE, AP3216C_ADDR) < 0) 
    {
        perror("ioctl I2C_SLAVE");
        close(i2c_fd);
        return -1;
    }
    
    // 3. Soft reset
    i2c_write_reg(AP3216C_SYSTEMCONG, 0x04);
    usleep(50000);  // Wait 50ms
    
    // 4. Set to ALS+PS+IR mode
    i2c_write_reg(AP3216C_SYSTEMCONG, 0x03);
    usleep(150000); // Wait 150ms
    
    printf("AP3216C initialized\n");
    return 0;
}

// Read sensor data
int ap3216c_read(sensor_data_t *data) 
{
    unsigned char buf[6];
    
    // Read 6 bytes from 0x0A
    if (i2c_read_reg(AP3216C_IRDATALOW, buf, 6) < 0) {
        return -1;
    }
    
    // Parse IR data (0x0A, 0x0B)
    data->ir = ((buf[1] & 0x3F) << 8) | buf[0];
    
    // Parse ALS data (0x0C, 0x0D)
    data->als = (buf[3] << 8) | buf[2];
    
    // Parse PS data (0x0E, 0x0F)
    data->ps = ((buf[5] & 0x0F) << 8) | buf[4];
    
    return 0;
}

// Close device
void ap3216c_close(void) 
{
    if (i2c_fd >= 0) 
    {
        close(i2c_fd);
        i2c_fd = -1;
    }
}