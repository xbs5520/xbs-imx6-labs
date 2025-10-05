#include "bsp_i2c.h"

void i2c_init(I2C_Type *base)
{
    base->I2CR &= ~(1 << 7);    // disable I2C
                                // IPG_CLK_ROOT = 66 MHZ
    base->IFDR = 0x15;          // devide 640 66000000 / 640 = 103.125 khz
    base->I2CR |= (1 << 7);     // enable I2C
}

// start from master
unsigned char i2c_master_start(I2C_Type *base, unsigned char address, 
                              enum i2c_direction direction)
{
    if(base->I2SR & (1 << 5)) // busy
        return 1;
    
    // set to send mode
    base->I2CR |= (1 << 5) | (1 << 4); 
  
    // start signal 
    base->I2DR =  ((unsigned int)address << 1) | ((direction == kI2C_Read)? 1:0);
    return 0;
}

// stop from master
unsigned char i2c_master_stop(I2C_Type *base)
{
    unsigned short timeout = 0xffff;

    base->I2CR &= ~((1 << 5) | (1 << 4)| (1 << 3));

    // wait for i2c busy
    while((base->I2SR & (1 << 5))){
        timeout--;
        if(timeout == 0)    //time out 
            return I2C_STATUS_TIMEOUT;
    }
    return I2C_STATUS_OK;
}

// repeated start signal
unsigned char i2c_master_repeated_start(I2C_Type *base, unsigned char address, 
                              enum i2c_direction direction)
{
    // busy or slave used
    if(base->I2SR & (1 << 5) && (((base->I2CR) & (1 << 5)) == 0))
        return 1;

    base->I2CR |=(1 << 4) | (1 << 2);

    // set slave address
    base->I2DR =  ((unsigned int)address << 1) | ((direction == kI2C_Read)? 1:0);
    return I2C_STATUS_OK;
}


unsigned char i2c_check_and_clear_error(I2C_Type *base, unsigned int status)
{
    if(status & (1 << 4)) {
        base->I2SR &= ~(1 << 4);        // clear
        base->I2CR &= ~(1 << 7);        // disable I2C
        base->I2CR |= (1 << 7);         // enable again
        return I2C_STATUS_ARBITRATIONLOST;
    }
    else if(status & (1 << 0)){         // no ack
        return I2C_STATUS_NAK;
    }
    return I2C_STATUS_OK;
}

// send data
void i2c_master_write(I2C_Type *base, const unsigned char *buf, 
                        unsigned int size)
{
    //wait for trans
    while(!(base->I2SR & (1 << 7)));

    base->I2SR &= ~(1 << 1); 
    base->I2CR |= 1 << 4; 

    while(size--)
    {
        base->I2DR = *buf++;  // write to I2DR

        while(!(base->I2SR & (1 << 1)));    // wait for trans
        base->I2SR &= ~(1<<1);

        // check ack
        if(i2c_check_and_clear_error(base, base->I2SR))
            break;
    }

    base->I2SR &= ~(1 << 1);
    i2c_master_stop(base);
}

void i2c_master_read(I2C_Type *base, unsigned char *buf, unsigned size)
{
    volatile uint8_t dummy = 0;
    dummy++; 

    // wait for trans
    while(!(base->I2SR & (1 << 7)));

    base->I2SR &= ~(1 << 1);
    base->I2CR &= ~((1 << 4) | (1 << 3));

    if(size == 1)
        base->I2CR |= (1 << 3);
    
    dummy = base->I2DR;

    while(size--) {
        while(!(base->I2SR & (1 << 1)));
        base->I2SR &= ~(1 << 1);

        if(size == 0) {
            i2c_master_stop(base);
        }   

        if(size == 1) {  
            base->I2CR |= (1 << 3);
        }
        *buf++ = base->I2DR;
    }
}

unsigned char i2c_master_transfer(I2C_Type *base, struct i2c_transfer *xfer)
{
	unsigned char ret = 0;
	 enum i2c_direction direction = xfer->direction;	

	base->I2SR &= ~((1 << 1) | (1 << 4));

	while(!((base->I2SR >> 7) & 0X1)){}; 

    if ((xfer->subaddressSize > 0) && (xfer->direction == kI2C_Read))
    {
        direction = kI2C_Write;
    }

	ret = i2c_master_start(base, xfer->slaveAddress, direction);
    if(ret)
    {	
		return ret;
	}

	while(!(base->I2SR & (1 << 1))){};

    ret = i2c_check_and_clear_error(base, base->I2SR);
    if(ret)
    {
      	i2c_master_stop(base);
        return ret;
    }
	
    if(xfer->subaddressSize)
    {
        do
        {
			base->I2SR &= ~(1 << 1);
            xfer->subaddressSize--;
			
            base->I2DR =  ((xfer->subaddress) >> (8 * xfer->subaddressSize));
  
			while(!(base->I2SR & (1 << 1)));

            ret = i2c_check_and_clear_error(base, base->I2SR);
            if(ret)
            {
             	i2c_master_stop(base); 			
             	return ret;
            }  
        } while ((xfer->subaddressSize > 0) && (ret == I2C_STATUS_OK));

        if(xfer->direction == kI2C_Read)        // read
        {
            base->I2SR &= ~(1 << 1);
            i2c_master_repeated_start(base, xfer->slaveAddress, kI2C_Read);
    		while(!(base->I2SR & (1 << 1))){};  //wait for trans

            // check error
			ret = i2c_check_and_clear_error(base, base->I2SR);
            if(ret)
            {
             	ret = I2C_STATUS_ADDRNAK;
                i2c_master_stop(base); 		// send stop
                return ret;  
            }
           	          
        }
    }	


    // write
    if ((xfer->direction == kI2C_Write) && (xfer->dataSize > 0))
    {
    	i2c_master_write(base, xfer->data, xfer->dataSize);
	}

    // read
    if ((xfer->direction == kI2C_Read) && (xfer->dataSize > 0))
    {
       	i2c_master_read(base, xfer->data, xfer->dataSize);
	}
	return 0;	
}
