/*
 * Driver shit for the Si3216 chipset.  
 * Userspace.  
 * For the Raspberry Pi.
 *
 * Developed as part of the Time Telephone, an art project for Burning
 * Man 2019.
 */

#include <stdio.h>

#include <bcm2835.h>

int main(int argc, char *argv[])
{
    int ret;
    ret = bcm2835_init();
    printf("init gives %d\n", ret);
    ret = bcm2835_spi_begin();
    printf("begin gives %d\n", ret);

    bcm2835_spi_end();
}
