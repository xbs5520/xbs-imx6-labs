#ifndef SENSOR_AP3216C_H
#define SENSOR_AP3216C_H

#define AP3216C_ADDR    0x1E
#define I2C_DEVICE      "/dev/i2c-0"

// Register addresses
// System config
#define AP3216C_SYSTEMCONG  0x00  // System config
#define AP3216C_INTSTATUS   0x01  // Interrupt status
#define AP3216C_INTCLEAR    0x02  // Interrupt clear
#define AP3216C_IRDATALOW   0x0A  // IR data low
#define AP3216C_IRDATAHIGH  0x0B  // IR data high
#define AP3216C_ALSDATALOW  0x0C  // ALS data low
#define AP3216C_ALSDATAHIGH 0x0D  // ALS data high
#define AP3216C_PSDATALOW   0x0E  // PS data low
#define AP3216C_PSDATAHIGH  0x0F  // PS data high

typedef struct {
    unsigned short ir;   // IR data
    unsigned short als;  // Ambient light
    unsigned short ps;   // Proximity
} sensor_data_t;

int ap3216c_init(void);
int ap3216c_read(sensor_data_t *data);
void ap3216c_close(void);

#endif