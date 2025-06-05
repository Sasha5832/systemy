#define FCY 4000000UL          
#include <xc.h>
#include <stdint.h>
#include <libpic30.h>

#pragma config POSCMOD = NONE, FNOSC = FRC
#pragma config FCKSM = CSDCMD, IESO = OFF, OSCIOFNC = OFF
#pragma config WDTPS = PS32768, FWPSA = PR128, FWDTEN = OFF, WINDIS = ON
#pragma config ICS   = PGx2, GWRP = OFF,  GCP = OFF,  JTAGEN = OFF

#define ALARM_BLINK_TIME 5        
#define BLINK_INTERVAL_MS 500     
#define POT_CHANNEL 5             

volatile uint8_t alarmActive = 0;
volatile uint8_t blinkPhase = 0;
volatile uint8_t allLedsOn = 0;
volatile uint16_t blinkCounter = 0;
volatile uint16_t blinkTimeSec = 0;
volatile uint8_t btnFlag = 0;             
volatile uint16_t debounceBtn = 0;       

static void initIO(void);
static void initADC(void);
static uint16_t readPot(void);
static void initTimer1(void);
static void initCN(void);
static void resetAlarm(void);

static void initIO(void)
{
    TRISA = 0x0000; 
    TRISD |= (1 << 6); 
    AD1PCFG = 0xFFFF;
    AD1PCFGbits.PCFG5 = 0; 
    TRISBbits.TRISB5 = 1;
}

static void initADC(void)
{
    AD1CON1 = 0;
    AD1CON2 = 0;
    AD1CON3 = 0x1F02;
    AD1CHS  = 0x0005;
    AD1CON1bits.ADON = 1;
}

static uint16_t readPot(void)
{
    AD1CON1bits.SAMP = 1;
    __delay_us(10);
    AD1CON1bits.SAMP = 0;
    while (!AD1CON1bits.DONE);
    return ADC1BUF0 & 0x03FF;
}

static void initTimer1(void)
{
    T1CON = 0x8030; 
    TMR1  = 0;
    PR1   = (uint16_t)(FCY/256/2 - 1);
    _T1IF = 0;
    _T1IP = 2;
    _T1IE = 1;
}

void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void)
{
    _T1IF = 0;

    if (debounceBtn > 0) debounceBtn--;

    if (alarmActive) {
        if (!allLedsOn) {
            blinkPhase = !blinkPhase;
            if (blinkPhase) {
                LATA = 0x01; 
            } else {
                LATA = 0x00; 
            }
            blinkCounter++;
            if (blinkCounter * BLINK_INTERVAL_MS >= ALARM_BLINK_TIME * 1000) {
                allLedsOn = 1;
                LATA = 0xFF; 
            }
        }
    }
}

void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void)
{
    IFS1bits.CNIF = 0;
    if (PORTDbits.RD6 == 0 && debounceBtn == 0) {
        btnFlag = 1;
        debounceBtn = 10;
    }
}

static void initCN(void)
{
    CNEN2bits.CN16IE = 1;  
    IFS1bits.CNIF = 0;
    IEC1bits.CNIE = 1;
    IPC4bits.CNIP = 4;
}

static void resetAlarm(void)
{
    alarmActive = 0;
    allLedsOn = 0;
    blinkCounter = 0;
    blinkPhase = 0;
    LATA = 0;
    btnFlag = 0;
}

int main(void)
{
    initIO();
    initADC();
    initTimer1();
    initCN();

    uint16_t alarmThreshold = 0x03FF / 2; 

    while (1)
    {
        uint16_t pot = readPot();

        if (!alarmActive && pot >= alarmThreshold) {
            alarmActive = 1;
            allLedsOn = 0;
            blinkCounter = 0;
            blinkPhase = 0;
            LATA = 0;
        }

        if (alarmActive) {
            if (pot < alarmThreshold || btnFlag) {
                resetAlarm();
            }
        }
    }
}
