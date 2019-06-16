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

#define MOD_TYPE_FXS	0
#define MOD_TYPE_FXO	1

/* Need to assert chipselect lead CS0 or CS1
 *
 * chip select for the fxo module Si3050 is SPI0_CSn0 -> gpio27 = P1_13
 *
 * chip select for the fxs module Si3016 is SPI0_CSn1, which looks
 * like it is tied to 3v3 by a pullup; also to -> gpio16 = P1_36
 *
 * reset for both is gpio17
 */

#define CS_NONE
#define CODEC_CS0 16//cs0
#define CODEC_CS1 27 //cs1
#define CODEC_CS2 23//cs2
#define CODEC_CS3 12 //cs3
#define CODEC_RESET 17

#define MODULE_LED0 24
#define MODULE_LED1 3
#define MODULE_LED2 2
#define MODULE_LED3 4


/* also there is one shared pcm highway
 *
 * SPI1 MOSI = PCMIN = PCM0_DIN = DTX = pcm from the line 
 * SPI1 SCLK = PCMOUT = PCM0_DOUT = DRX = pcm to the line
 * SPI1 MISO = PCMFS = PCM0_DFSC = FSYNC = 
 * GPIO_18 = PCMCLK = PCM_DCLK = PCLK = 4.096 MHz crystal
 */

static void wctdm_setreg(int modtype, unsigned char reg, unsigned char value);
static unsigned char wctdm_getreg(int modtype, unsigned char reg);

int main(int argc, char *argv[])
{
    int ret;
    ret = bcm2835_init();
    printf("init gives %d\n", ret);
    ret = bcm2835_spi_begin();
    printf("begin gives %d\n", ret);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);
    bcm2835_gpio_fsel(CODEC_CS0, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(CODEC_CS0);
    bcm2835_gpio_fsel(CODEC_CS1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(CODEC_CS1);
    bcm2835_gpio_fsel(CODEC_CS2, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(CODEC_CS2);
    bcm2835_gpio_fsel(CODEC_CS3, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(CODEC_CS3);

    bcm2835_gpio_fsel(MODULE_LED3, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(MODULE_LED3);

//    bcm2835_gpio_set(CODEC_RESET);
//    bcm2835_gpio_clr(CODEC_RESET);

    ret = wctdm_getreg(CODEC_CS0, 81);
    printf("read reg gives %d\n", ret);

    bcm2835_spi_end();
    bcm2835_close();
}


static void wctdm_setreg(int cs, unsigned char reg, unsigned char value)
{
    char bytes[2] = { reg & 0x7f, value };
    bcm2835_gpio_clr(cs);
    bcm2835_spi_writenb(bytes, 2);
    bcm2835_gpio_set(cs);
    return;
}

static unsigned char wctdm_getreg(int cs, unsigned char reg)
{
    char bytes[2] = { reg | 0x80, 0 };
    bcm2835_gpio_clr(cs);
    bcm2835_spi_transfern(bytes, 2);
    bcm2835_gpio_set(cs);
    printf("read reg gives %hhd %hhd\n", bytes[0], bytes[1]);
    return bytes[1];
}


