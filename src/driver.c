/*
 * Driver shit for the Si3216 chipset.  
 * Userspace.  
 * For the Raspberry Pi.
 *
 * Developed as part of the Time Telephone, an art project for Burning
 * Man 2019.
 */

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#define SECONDS      1000*1000*1000
#define MILLISECONDS 1000*1000
#define MICROSECONDS 1000
#define NANOSECONDS  1
#include <errno.h>

#include <bcm2835.h>

#define MOD_TYPE_FXS	0
#define MOD_TYPE_FXO	1

#define DEBUG

#ifdef DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#else /* no debug */
#define dprintf(...) asm("nop")
#endif

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

#define REG_IDA_LO 28
#define REG_IDA_HI 29
#define REG_IAA 30
#define REG_ISTATUS 31

#define NUM_CAL_REGS 12
uint8_t calregs[NUM_CAL_REGS];
enum {
    PROSLIC_POWER_UNKNOWN,
    PROSLIC_POWER_ON,
    PROSLIC_POWER_WARNED,
} proslic_power;

int loopcurrent = 20;

/* some shit i ganked from pitdm.c */
struct alpha {
    int reg10;
    int reg16;
    char* regname;
    uint16_t defval;
};
static struct alpha indirect_regs[] =
{
{0,255,"DTMF_ROW_0_PEAK",0x55C2},
{1,255,"DTMF_ROW_1_PEAK",0x51E6},
{2,255,"DTMF_ROW2_PEAK",0x4B85},
{3,255,"DTMF_ROW3_PEAK",0x4937},
{4,255,"DTMF_COL1_PEAK",0x3333},
{5,255,"DTMF_FWD_TWIST",0x0202},
{6,255,"DTMF_RVS_TWIST",0x0202},
{7,255,"DTMF_ROW_RATIO_TRES",0x0198},
{8,255,"DTMF_COL_RATIO_TRES",0x0198},
{9,255,"DTMF_ROW_2ND_ARM",0x0611},
{10,255,"DTMF_COL_2ND_ARM",0x0202},
{11,255,"DTMF_PWR_MIN_TRES",0x00E5},
{12,255,"DTMF_OT_LIM_TRES",0x0A1C},
{13,0,"OSC1_COEF",0x7B30},
{14,1,"OSC1X",0x0063},
{15,2,"OSC1Y",0x0000},
{16,3,"OSC2_COEF",0x7870},
{17,4,"OSC2X",0x007D},
{18,5,"OSC2Y",0x0000},
{19,6,"RING_V_OFF",0x0000},
{20,7,"RING_OSC",0x7EF0},
{21,8,"RING_X",0x0160},
{22,9,"RING_Y",0x0000},
{23,255,"PULSE_ENVEL",0x2000},
{24,255,"PULSE_X",0x2000},
{25,255,"PULSE_Y",0x0000},
//{26,13,"RECV_DIGITAL_GAIN",0x4000},	// playback volume set lower
{26,13,"RECV_DIGITAL_GAIN",0x2000},	// playback volume set lower
{27,14,"XMIT_DIGITAL_GAIN",0x4000},
//{27,14,"XMIT_DIGITAL_GAIN",0x2000},
{28,15,"LOOP_CLOSE_TRES",0x1000},
{29,16,"RING_TRIP_TRES",0x3600},
{30,17,"COMMON_MIN_TRES",0x1000},
{31,18,"COMMON_MAX_TRES",0x0200},
{32,19,"PWR_ALARM_Q1Q2",0x07C0},
{33,20,"PWR_ALARM_Q3Q4",0x2600},
{34,21,"PWR_ALARM_Q5Q6",0x1B80},
{35,22,"LOOP_CLOSURE_FILTER",0x8000},
{36,23,"RING_TRIP_FILTER",0x0320},
{37,24,"TERM_LP_POLE_Q1Q2",0x008C},
{38,25,"TERM_LP_POLE_Q3Q4",0x0100},
{39,26,"TERM_LP_POLE_Q5Q6",0x0010},
{40,27,"CM_BIAS_RINGING",0x0C00},
{41,64,"DCDC_MIN_V",0x0C00},
{42,255,"DCDC_XTRA",0x1000},
{43,66,"LOOP_CLOSE_TRES_LOW",0x1000},
};


/* also there is one shared pcm highway
 *
 * SPI1 MOSI = PCMIN = PCM0_DIN = DTX = pcm from the line 
 * SPI1 SCLK = PCMOUT = PCM0_DOUT = DRX = pcm to the line
 * SPI1 MISO = PCMFS = PCM0_DFSC = FSYNC = 
 * GPIO_18 = PCMCLK = PCM_DCLK = PCLK = 4.096 MHz crystal
 */

void proslic_setreg(int cs, uint8_t reg, uint8_t value);
unsigned char proslic_getreg(int cs, uint8_t reg);
static bool spinwait_indirect_access(int cs, bool want_result);
bool proslic_setreg_indirect(int cs, uint8_t reg, uint16_t value);
int proslic_getreg_indirect(int cs, uint8_t reg);

int proslic_reset(int cs, bool hw, bool sw);
int proslic_initialize(int cs);
int proslic_calibrate(int cs);
int proslic_setup(int cs);

static int wctdm_init_proslic(int cs, int fast, int manual, int sane);
static int proslic_is_insane(int cs);
static int proslic_manual_calibrate(int cs);
static int proslic_auto_calibrate(int cs);
static int powerup_proslic(int cs, int fast);
static int proslic_powerleak_test(int cs);
static bool proslic_init_indirect_regs(int cs);
static int proslic_verify_indirect_regs(int cs);

inline long int timesince(struct timespec *start)
{
    struct timespec now;
    struct timespec diff;
    clock_gettime(CLOCK_REALTIME, &now);

    if (now.tv_nsec < start->tv_nsec) {
        now.tv_sec -= 1;
        now.tv_nsec += 1*SECONDS;
    }
    diff.tv_sec = now.tv_sec - start->tv_sec;
    diff.tv_nsec = now.tv_nsec - start->tv_nsec;
    
    return diff.tv_sec * SECONDS + diff.tv_nsec;
}

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

    //bcm2835_gpio_set(CODEC_RESET);
    //bcm2835_gpio_clr(CODEC_RESET);

    ret = proslic_getreg(CODEC_CS0, 0);
    printf("read reg 0 gives %d\n", ret);

    ret = proslic_getreg(CODEC_CS0, 1);
    printf("read reg 1 gives %d\n", ret);

    ret = proslic_getreg(CODEC_CS0, 6);
    printf("read reg 6 gives %d\n", ret);

    proslic_reset(CODEC_CS0, true, true);
    ret = wctdm_init_proslic(CODEC_CS0, 0, 1, 0);
    if (ret != 0) printf("initialization failed at step %d\n", ret);

/*
    ret = proslic_getreg(CODEC_CS0, 81);
    printf("read reg 81 gives %d\n", ret);

    ret = proslic_getreg_indirect(CODEC_CS0, 0);
    printf("read reg i0 gives %d\n", ret);
*/

    bcm2835_spi_end();
    bcm2835_close();
}


void proslic_setreg(int cs, uint8_t reg, uint8_t value)
{
    char bytes[2] = { reg & 0x7f, value };
    dprintf(" - setting reg %d to %hhx\n", reg, value);
    bcm2835_gpio_clr(cs);
    bcm2835_spi_writenb(bytes, 2);
    bcm2835_gpio_set(cs);
    return;
}

unsigned char proslic_getreg(int cs, uint8_t reg)
{
    char bytes[2] = { reg | 0x80, 0 };
    bcm2835_gpio_clr(cs);
    bcm2835_spi_transfern(bytes, 2);
    bcm2835_gpio_set(cs);
    dprintf(" - getreg %d gives %hhx %hhx\n", reg, bytes[0], bytes[1]);
    return bytes[1];
}

/* wait for the indirect access to complete.  check a few thousand
 * times and then give up.
 */
static bool spinwait_indirect_access(int cs, bool want_result)
{
    int count = 6000;
    unsigned char data;
    while (--count >= 0) {
        data = proslic_getreg(cs, REG_ISTATUS);
        if (want_result) {
            /* looking for a "something is here for you" */
            if (data == 1) break;
        } else {
            /* looking for availability to submit a request */
            if (data == 0) break;
        }
    }

    if (count <= 0) {
        printf(" ##### Loop error (%02x) #####\n", data);
        return false;
    }
    return true;
}

bool proslic_setreg_indirect(int cs, uint8_t address, uint16_t value)
{
    unsigned long flags;
    int res = false;
    dprintf("setting indirectly [%x]=%x\n", (int) address, (int) value);
    if (true == spinwait_indirect_access(cs, false)) {
        proslic_setreg(cs, REG_IDA_LO, (unsigned char)(value & 0xFF));
        proslic_setreg(cs, REG_IDA_HI, (unsigned char)((value & 0xFF00)>>8));
        proslic_setreg(cs, REG_IAA, address);
        res = true;
    };
    return res;
}

int proslic_getreg_indirect(int cs, uint8_t address)
{
    unsigned long flags;
    int res = -1;
    char *p = NULL;
    dprintf("getting indirectly [%d]\n", (int) address);
    if (address == 0xff) {
        return 0;
    }
        
    if (true == spinwait_indirect_access(cs, false)) {
        proslic_setreg(cs, REG_IAA, address);
        if (true == spinwait_indirect_access(cs, true)) {
            unsigned char data1, data2;
            data1 = proslic_getreg(cs, REG_IDA_LO);
            data2 = proslic_getreg(cs, REG_IDA_HI);
            res = data1 | (data2 << 8);
        } else {
            p = "indirect access never finished\n";
        }
    } else {
        p = "chip not ready to start\n";
    }
    if (p) {
        printf("%s", p);
    }
    return res;
}

int proslic_reset(int cs, bool hw, bool sw)
{
    struct timespec delay = {0, 0};

    int step = 0;
    int data = 0;

    /* 1. Hold RESET low, and apply power.
     * 2. PCLK and FS must be present and stable.
     * 3. Preset CS and SCLK to high state (if SCLK is to be static
     *    between CSs).
     * 4. CS should be deasserted a minimum of 250 ns between access
     *    bytes.
     * 5. Release RESET.
     */

    if (hw) {
        step = 1;
        printf("step 1: hardware reset\n");
        bcm2835_gpio_clr(CODEC_RESET);
        bcm2835_gpio_set(cs);
        delay.tv_sec = 0;
        delay.tv_nsec = 250*MILLISECONDS;
        if (nanosleep(&delay, NULL) == -1) return step;
        bcm2835_gpio_set(CODEC_RESET);
    }

    if (sw) {
        step = 1;
        /* software reset? according to the software i guess */
        printf("step 1: software reset\n");
        proslic_setreg(cs, 1, 0x80);
    }

    /* 6. Wait 2 ms (16 FS) after releasing RESET before communicating
     *    to the ProSLIC.
     */
    step = 6;
    delay.tv_nsec = 200*MILLISECONDS;
    if (nanosleep(&delay, NULL) == -1) return step;

    return 0;
}

/* phase 1 of chip initialization */
int proslic_initialize(int cs)
{
    struct timespec delay = {0, 0};

    int step = 0;
    int data = 0;

    /* 6. Wait 2 ms (16 FS) after releasing RESET before communicating
     *    to the ProSLIC.
     */
    step = 6;
    delay.tv_nsec = 200*MILLISECONDS;
    if (nanosleep(&delay, NULL) == -1) return step;

    /* 7. Set Daisy Chain mode if used (direct register 0, 3- byte
     *    access from here on in daisy chain mode).
     * 8. Read direct registers 8, 11, and 64 to verify communication
     *    with ProSLIC values should be 0x02, 0x33, and 0x00.
     */
    step = 8;
    printf("step 8: check that communication happens\n");
    data = proslic_getreg(cs, 6);
    if (data != 0x80)  {
        printf("expect [8] = 0x80, got %x\n", (int)data);
        return step;
    }
    data = proslic_getreg(cs, 11);
    if (data != 0x33) {
        printf("expect [11] = 0x33, got %x\n", (int)data);
        return step;
    }
    data = proslic_getreg(cs, 64);
    if (data != 0x11) {
        printf("expect [64] = 0x00, got %x\n", (int)data);
        return step;
    }


    return 0;
}

/* phase 2 of chip initialization */
int proslic_calibrate(int cs)
{
}

/* phase 3 of chip initialization (last phase) */
int proslic_setup(int cs)
{
}




static int wctdm_init_proslic(int cs, int fast, int manual, int sane)
{
    unsigned short tmp[5];
    unsigned char r19,r9;
    int x;
    int fxsmode=0;
    manual = 1;

    /* Sanity check the ProSLIC */
    if (!sane && proslic_is_insane(cs))
        return -2;

    if (sane) {
        /* Make sure we turn off the DC->DC converter to prevent anything from blowing up */
        proslic_setreg(cs, 14, 0x10);
    }

    if (false == proslic_init_indirect_regs(cs)) {
        dprintf("Indirect Registers failed to initialize on module %d.\n", cs);
        return -1;
    }

    /* Clear scratch pad area */
    //proslic_setreg_indirect(cs, 97, 0);

    /* Clear digital loopback */
    proslic_setreg(cs, 8, 0);

    /* Revision C optimization */
    proslic_setreg(cs, 108, 0xeb);

    /* Disable automatic VBat switching for safety to prevent
       Q7 from accidently turning on and burning out. */
    proslic_setreg(cs, 67, 0x07);  /* Note, if pulse dialing has problems at high REN loads
                                      change this to 0x17 */

    /* Turn off Q7 */
    proslic_setreg(cs, 66, 1);

    /* Flush ProSLIC digital filters by setting to clear, while
       saving old values */
    dprintf("clearing filterbank\n");
    for (x = 0; x < 5; x++) {
        tmp[x] = proslic_getreg_indirect(cs, x + 22);
        proslic_setreg_indirect(cs, x + 22, 0x8000);
    }

    /* Power up the DC-DC converter */
    if (powerup_proslic(cs, fast)) {
        dprintf("Unable to do INITIAL ProSLIC powerup on module %d\n", cs);
        return -1;
    }

    if (!fast) {

        /* Check for power leaks */
        if (proslic_powerleak_test(cs)) {
            dprintf("ProSLIC module %d failed leakage test.  Check for short circuit\n", cs);
        }
        /* Power up again */
        if (powerup_proslic(cs, fast)) {
            dprintf("Unable to do FINAL ProSLIC powerup on module %d\n", cs);
            return -1;
        }
#ifndef NO_CALIBRATION
        /* Perform calibration */
        if(manual) {
            if (proslic_manual_calibrate(cs)) {
                dprintf("Proslic failed on Manual Calibration\n");
                if (proslic_manual_calibrate(cs)) {
                    dprintf("Proslic Failed on Second Attempt to Calibrate Manually. (Try -DNO_CALIBRATION in Makefile)\n");
                    return -1;
                }
                dprintf("Proslic Passed Manual Calibration on Second Attempt\n");
            }
        }
        else {
            if(proslic_auto_calibrate(cs))  {
                dprintf("ProSlic died on Auto Calibration.\n");
                if (proslic_auto_calibrate(cs)) {
                    dprintf("Proslic Failed on Second Attempt to Auto Calibrate\n");
                    return -1;
                }
                dprintf("Proslic Passed Auto Calibration on Second Attempt\n");
            }
        }
        /* Perform DC-DC calibration */
        proslic_setreg(cs, 93, 0x99);
        r19 = proslic_getreg(cs, 107);
        if ((r19 < 0x2) || (r19 > 0xd)) {
            dprintf("DC-DC cal has a surprising direct 107 of 0x%02x!\n", r19);
            proslic_setreg(cs, 107, 0x8);
        }

        /* Save calibration vectors */
        for (x = 0; x < NUM_CAL_REGS; x++)
            calregs[x] = proslic_getreg(cs, 96 + x);
#endif

    } else {
        /* Restore calibration registers */
        for (x = 0; x < NUM_CAL_REGS; x++)
            proslic_setreg(cs, 96 + x, calregs[x]);
    }
    /* Calibration complete, restore original values */
    for (x = 0; x < 5; x++) {
        proslic_setreg_indirect(cs, x + 22, tmp[x]);
    }

    if (proslic_verify_indirect_regs(cs)) {
        dprintf("Indirect Registers failed verification.\n");
        return -1;
    }

#if 0
    /* Disable Auto Power Alarm Detect and other "features" */
    proslic_setreg(cs, 67, 0x0e);
    blah = proslic_getreg(cs, 67);
#endif

    // U-Law 8-bit interface
    proslic_setreg(cs, 2, 0);    // Tx Start count low byte  0
    //proslic_setreg(cs, 2, (3-card) * 8);    // Tx Start count low byte  0
    proslic_setreg(cs, 3, 0);    // Tx Start count high byte 0
    proslic_setreg(cs, 4, 0);    // Rx Start count low byte  0
    proslic_setreg(cs, 5, 0);    // Rx Start count high byte 0
    proslic_setreg(cs, 18, 0xff);     // clear all interrupt
    proslic_setreg(cs, 19, 0xff);
    proslic_setreg(cs, 20, 0xff);
    proslic_setreg(cs, 73, 0x04);

    dprintf("dump out\n reg2 %x\n reg3 %x\n reg4 %x\n reg5 %x\n", proslic_getreg(cs, 2), proslic_getreg(cs, 3),proslic_getreg(cs, 4),proslic_getreg(cs, 5));

#if 0
    proslic_setreg(cs, 21, 0x00); 	// enable interrupt
    proslic_setreg(cs, 22, 0x02); 	// Loop detection interrupt
    proslic_setreg(cs, 23, 0x01); 	// DTMF detection interrupt
#endif

#if 0
    /* Enable loopback */
    proslic_setreg(cs, 8, 0x2);
    proslic_setreg(cs, 14, 0x0);
    proslic_setreg(cs, 64, 0x0);
    proslic_setreg(cs, 1, 0x08);
#endif

    /* dyked out -æ
    if (wctdm_init_ring_generator_mode(cs)) {
        return -1;
    }
    */

    /* dyked out -æ
    if(fxstxgain || fxsrxgain) {
        r9 = proslic_getreg(cs, 9);
        switch (fxstxgain) {

        case 35:
            r9+=8;
            break;
        case -35:
            r9+=4;
            break;
        case 0:
            break;
        }

        switch (fxsrxgain) {

        case 35:
            r9+=2;
            break;
        case -35:
            r9+=1;
            break;
        case 0:
            break;
        }
        proslic_setreg(cs, 9, r9);
    }
    */

    dprintf("DEBUG: fxstxgain:%s fxsrxgain:%s\n",
            ((proslic_getreg(cs, 9)/8) == 1)?"3.5":(((proslic_getreg(cs, 9)/4) == 1)?"-3.5":"0.0"),
            ((proslic_getreg(cs, 9)/2) == 1)?"3.5":((proslic_getreg(cs, 9)%2)?"-3.5":"0.0"));

    //fxs->lasttxhook = fxs->idletxhookstate;
    //proslic_setreg(cs, LINE_STATE, fxs->lasttxhook);
    return 0;
}

static int proslic_is_insane(int cs)
{
    int blah, insane_report;
    insane_report = 1;

    blah = proslic_getreg(cs, 0);
    dprintf( "ProSLIC on module %d, product %d, version %d\n", cs, (blah & 0x30) >> 4, (blah & 0xf));

    if (((blah & 0xf) == 0) || ((blah & 0xf) == 0xf)) {
        /* SLIC not loaded */
        return -1;
    }
    if ((blah & 0xf) < 2) {
        dprintf( "ProSLIC 3210 version %d is too old\n", blah & 0xf);
        return -1;
    }
    if (! (proslic_getreg(cs, 1) & 0x80)) {
        dprintf("ProSLIC 3215, not a 3210\n");
        return -1;
    }

    proslic_setreg(cs, 8, 2);
    blah = proslic_getreg(cs, 8);
    if (insane_report) dprintf( "ProSLIC on module %d Reg 8 Reads %d Expected is 0x2\n",cs,blah);
    if (blah != 0x2) {
        dprintf( "ProSLIC on module %d insane (1) %d should be 2\n", cs, blah);
        return -1;
    }

    proslic_setreg(cs, 64, 0);
    blah = proslic_getreg(cs, 64);
    if (insane_report) dprintf( "ProSLIC on module %d Reg 64 Reads %d Expected is 0x0\n", cs, blah);
    if (blah != 0x0) {
        dprintf( "ProSLIC on module %d insane (2)\n", cs);
        return -1;
    }

    blah = proslic_getreg(cs, 11);
    if (insane_report) dprintf( "ProSLIC on module %d Reg 11 Reads %d Expected is 0x33\n", cs, blah);
    if (blah != 0x33) {
        dprintf( "ProSLIC on module %d insane (3)\n", cs);
        return -1;
    } 

    /* Just be sure it's setup right. */
    proslic_setreg(cs, 30, 0);

    dprintf( "ProSLIC on module %d seems sane.\n", cs);
    return 0;
}

static int proslic_manual_calibrate(int cs)
{
    struct timespec start;
    struct timespec delay;
    unsigned char i;

    proslic_setreg(cs, 21, 0); //(0)  Disable all interupts in DR21
    proslic_setreg(cs, 22, 0); //(0)  Disable all interupts in DR21
    proslic_setreg(cs, 23, 0); //(0)  Disable all interupts in DR21
    proslic_setreg(cs, 64, 0); //(0)

    proslic_setreg(cs, 97, 0x18); // (0x18) Calibrations without the ADC and DAC offset and without common mode calibration.
    proslic_setreg(cs, 96, 0x47); // (0x47)	Calibrate common mode and differential DAC mode DAC + ILIM

    clock_gettime(CLOCK_REALTIME, &start);
    while (proslic_getreg(cs, 96) != 0) {
        if (timesince(&start) > 800 * MILLISECONDS)
            return -1;
    }
//Initialized DR 98 and 99 to get consistant results.
// 98 and 99 are the results registers and the search should have same intial conditions.

/*******************************The following is the manual gain mismatch calibration****************************/
/*******************************This is also available as a function *******************************************/
    // Delay 10ms
    delay.tv_sec = 0; delay.tv_nsec = 10 * MILLISECONDS;
    if (nanosleep(&delay, NULL) == -1) return -1;
    
    proslic_setreg_indirect(cs, 88, 0);
    proslic_setreg_indirect(cs, 89, 0);
    proslic_setreg_indirect(cs, 90, 0);
    proslic_setreg_indirect(cs, 91, 0);
    proslic_setreg_indirect(cs, 92, 0);
    proslic_setreg_indirect(cs, 93, 0);

    proslic_setreg(cs, 98, 0x10); // This is necessary if the calibration occurs other than at reset time
    proslic_setreg(cs, 99, 0x10);

    for (i = 0x1f; i > 0; i--)
    {
        proslic_setreg(cs, 98, i);

        clock_gettime(CLOCK_REALTIME, &start);
        while (timesince(&start) < 40 * MILLISECONDS)
            ;
        if ((proslic_getreg(cs, 88)) == 0)
            break;
    } // for

    for (i = 0x1f; i > 0; i--)
    {
        proslic_setreg(cs, 99, i);

        clock_gettime(CLOCK_REALTIME, &start);
        while (timesince(&start) < 40 * MILLISECONDS)
            ;
        if ((proslic_getreg(cs, 89)) == 0)
            break;
    }//for

/*******************************The preceding is the manual gain mismatch calibration****************************/
/**********************************The following is the longitudinal Balance Cal***********************************/
    proslic_setreg(cs, 64, 1);

    clock_gettime(CLOCK_REALTIME, &start);
    while (timesince(&start) < 100 * MILLISECONDS)
            ;

    proslic_setreg(cs, 64, 0);
    proslic_setreg(cs, 23, 0x4);  // enable interrupt for the balance Cal
    proslic_setreg(cs, 97, 0x1); // this is a singular calibration bit for longitudinal calibration
    proslic_setreg(cs, 96, 0x40);

    proslic_getreg(cs, 96); /* Read Reg 96 just cause */

    proslic_setreg(cs, 21, 0xFF);
    proslic_setreg(cs, 22, 0xFF);
    proslic_setreg(cs, 23, 0xFF);

    /**The preceding is the longitudinal Balance Cal***/
    return(0);

}

static int proslic_auto_calibrate(int cs)
{
    struct timespec start;
    int x;
    /* Perform all calibrations */
    proslic_setreg(cs, 97, 0x1f);

    /* Begin, no speedup */
    proslic_setreg(cs, 96, 0x5f);

    /* Wait for it to finish */
    clock_gettime(CLOCK_REALTIME, &start);
    while (proslic_getreg(cs, 96)) {
        if (timesince(&start) > 500 * MILLISECONDS) {
            dprintf( "Timeout waiting for calibration of module %d\n", cs);
            return -1;
        }
    }

#ifdef DEBUG
    /* Print calibration parameters */
    dprintf( "Calibration Vector Regs 98 - 107: \n");
    for (x = 98; x < 108; x++) {
        dprintf("%d: %02x\n", x, proslic_getreg(cs, x));
    }
#endif
    return 0;
}

static int powerup_proslic(int cs, int fast)
{
    unsigned char vbat;
    int lim;
    struct timespec start;

    /* Set period of DC-DC converter to 1/64 khz */
    proslic_setreg(cs, 92, 0xff /* was 0xff */);

    /* Wait for VBat to powerup */
    clock_gettime(CLOCK_REALTIME, &start);

    /* Disable powerdown */
    proslic_setreg(cs, 14, 0);

    /* If fast, don't bother checking anymore */
    if (fast)
        return 0;

    while ((vbat = proslic_getreg(cs, 82)) < 0xc0) {
        if (timesince(&start) > 500 * MILLISECONDS) {
            break;
        }
    }

    if (vbat < 0xc0) {
        if (proslic_power == PROSLIC_POWER_UNKNOWN)
            dprintf( "ProSLIC on module %d failed to powerup within %d ms (%d mV only)\n\n -- DID YOU REMEMBER TO PLUG IN THE HD POWER CABLE TO THE TDM400P??\n",
                     cs,
                     (int)(timesince(&start) * MILLISECONDS),
                     vbat * 375);
        proslic_power = PROSLIC_POWER_WARNED;
        return -1;
    } else {
        dprintf( "ProSLIC on module %d powered up to -%d volts (%02x) in %d ms\n",
                 cs,
                 vbat * 376 / 1000,
                 vbat,
                 (int)(timesince(&start) * MILLISECONDS)
            );
    }
    proslic_power = PROSLIC_POWER_ON;

    /* Proslic max allowed loop current, reg 71 LOOP_I_LIMIT */
    /* If out of range, just set it to the default value     */
    lim = (loopcurrent - 20) / 3;
    if ( loopcurrent > 41 ) {
        lim = 0;
        dprintf( "Loop current out of range! Setting to default 20mA!\n");
    } else {
        dprintf( "Loop current set to %dmA!\n",(lim*3)+20);
    }
    proslic_setreg(cs, 71, lim);
    
    /* Engage DC-DC converter */
    proslic_setreg(cs, 93, 0x19 /* was 0x19 */);
//æ#if 0
    clock_gettime(CLOCK_REALTIME, &start);
    while (0x80 & proslic_getreg(cs, 93)) {
        if (timesince(&start) > 500*MILLISECONDS) {
            dprintf( "Timeout waiting for DC-DC calibration on module %d\n", cs);
            return -1;
        }
    }

//æ#if 0
    /* Wait a full two seconds */
    while (timesince(&start) < 2*SECONDS);

    /* Just check to be sure */
    vbat = proslic_getreg(cs, 82);
    dprintf( "ProSLIC on module %d powered up to -%d volts (%02x) in %d ms\n",
           cs, vbat * 376 / 1000, vbat,
             (int)(timesince(&start) * MILLISECONDS)
        );
//æ#endif
//æ#endif
    return 0;

}

static int proslic_powerleak_test(int cs)
{
    struct timespec start;
    unsigned char vbat;

    /* Turn off linefeed */
    proslic_setreg(cs, 64, 0);

    /* Power down */
    proslic_setreg(cs, 14, 0x10);

    /* Wait for one second */
    clock_gettime(CLOCK_REALTIME, &start);

    while((vbat = proslic_getreg(cs, 82)) > 0x6) {
        if (timesince(&start) > 500 * MILLISECONDS) {
            break;
        }
    }
        
    if (vbat < 0x06) {
        dprintf("Excessive leakage detected on module %d: %d volts (%02x) after %d ms\n", cs,
                376 * vbat / 1000,
                vbat,
                (int)(timesince(&start) * MILLISECONDS)
            );
        return -1;
    } else {
        dprintf("Post-leakage voltage: %d volts\n",
                376 * vbat / 1000);
    }
    return 0;
}

static bool proslic_init_indirect_regs(int cs)
{
    unsigned char i;

    dprintf("initializing initial indirect registers (max ord %d)\n",
            (sizeof(indirect_regs) / sizeof(indirect_regs[0]))
        );

    for (i = 0; i < (sizeof(indirect_regs) / sizeof(indirect_regs[0])); i++) {
        dprintf(" init reg %d (ord %d)\n", indirect_regs[i].reg16, i);
        if (indirect_regs[i].reg16 == 255) continue;
        if (false == proslic_setreg_indirect(cs, indirect_regs[i].reg16, indirect_regs[i].defval))
            return false;
    }

    return true;
}

static int proslic_verify_indirect_regs(int cs)
{
    int passed = 1;
    unsigned short i, initial;
    int j;

    for (i = 0; i < (sizeof(indirect_regs) / sizeof(indirect_regs[0])); i++) {
        if (indirect_regs[i].reg16 == 255) continue;
        if ((j = proslic_getreg_indirect(cs, indirect_regs[i].reg16)) < 0) {
            dprintf("Failed to read indirect register %d\n", indirect_regs[i].reg16);
            return -1;
        }
        initial= indirect_regs[i].defval;

        if ( (j != initial) && (indirect_regs[i].defval != 255) )
        {
            dprintf("!!!!!!! %s  iREG %X = %X  should be %X\n",
                    indirect_regs[i].regname, indirect_regs[i].reg16, j, initial );
            passed = 0;
        }
    }

    if (passed) {
        dprintf("Init Indirect Registers completed successfully.\n");
    } else {
        dprintf(" !!!!! Init Indirect Registers UNSUCCESSFULLY.\n");
        return -1;
    }
    return 0;
}

