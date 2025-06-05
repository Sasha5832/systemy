#define FCY 4000000UL          
#include <xc.h>
#include <stdint.h>
#include <libpic30.h>

#pragma config POSCMOD = NONE, FNOSC = FRC
#pragma config FCKSM = CSDCMD, IESO = OFF, OSCIOFNC = OFF
#pragma config WDTPS = PS32768, FWPSA = PR128, FWDTEN = OFF, WINDIS = ON
#pragma config ICS   = PGx2, GWRP = OFF,  GCP = OFF,  JTAGEN = OFF

#define ALARM_BLINK_TIME 5        // ile sekund mruga jedna dioda
#define BLINK_INTERVAL_MS 500     // mruganie co 0,5 s
#define POT_CHANNEL 5             // RB5 jako analog

volatile uint8_t alarmActive = 0;
volatile uint8_t blinkPhase = 0;
volatile uint8_t allLedsOn = 0;
volatile uint16_t blinkCounter = 0;
volatile uint16_t blinkTimeSec = 0;
volatile uint8_t btnFlag = 0;             // Flaga przycisku RD6
volatile uint16_t debounceBtn = 0;        // Debouncing softwarowy

static void initIO(void);
static void initADC(void);
static uint16_t readPot(void);
static void initTimer1(void);
static void initCN(void);
static void resetAlarm(void);

static void initIO(void)
{
    TRISA = 0x0000; // PORTA jako wyjście
    TRISD |= (1 << 6); // RD6 jako wejście
    AD1PCFG = 0xFFFF;
    AD1PCFGbits.PCFG5 = 0; // RB5 jako analog
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
    // Timer1: przerwanie co ~0,5s (500ms) dla mrugania
    T1CON = 0x8030; // prescaler 1:256, Timer ON
    TMR1  = 0;
    PR1   = (uint16_t)(FCY/256/2 - 1); // FCY/256 daje 15625 Hz, podzielić przez 2 na 500ms
    _T1IF = 0;
    _T1IP = 2;
    _T1IE = 1;
}

// Przerwanie Timer1 – obsługuje mruganie diody/alarmu oraz debouncing
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void)
{
    _T1IF = 0;

    // Debouncing przycisku RD6
    if (debounceBtn > 0) debounceBtn--;

    // Obsługa alarmu
    if (alarmActive) {
        if (!allLedsOn) {
            blinkPhase = !blinkPhase;
            if (blinkPhase) {
                LATA = 0x01; // zapal pierwszą diodę
            } else {
                LATA = 0x00; // zgaś diody
            }
            blinkCounter++;
            if (blinkCounter * BLINK_INTERVAL_MS >= ALARM_BLINK_TIME * 1000) {
                allLedsOn = 1;
                LATA = 0xFF; // zapal wszystkie diody
            }
        }
        // allLedsOn: diody zapalone cały czas
    }
}

// Przerwanie CN – obsługuje przycisk RD6
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void)
{
    IFS1bits.CNIF = 0;
    // Debouncing: przycisk działa tylko jeśli debounce == 0
    if (PORTDbits.RD6 == 0 && debounceBtn == 0) {
        btnFlag = 1;
        debounceBtn = 10; // blokada na ok. 5s/10*0.5s=5 sek. lub dostosuj do preskalera
    }
}

static void initCN(void)
{
    CNEN2bits.CN16IE = 1;   // RD6 (CN16)
    // Pull-up jeśli konieczny: CNPUDbits.CNPUD6 = 1;
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

    uint16_t alarmThreshold = 0x03FF / 2; // połowa zakresu potencjometru

    while (1)
    {
        uint16_t pot = readPot();

        // Jeśli alarm nieaktywny i przekroczono próg — uruchom alarm
        if (!alarmActive && pot >= alarmThreshold) {
            alarmActive = 1;
            allLedsOn = 0;
            blinkCounter = 0;
            blinkPhase = 0;
            LATA = 0;
        }

        // Jeśli alarm aktywny:
        if (alarmActive) {
            // Jeśli potencjometr spadnie poniżej progu lub naciśnięto przycisk — wyłącz alarm
            if (pot < alarmThreshold || btnFlag) {
                resetAlarm();
            }
        }
        // (Opcjonalnie) Możesz tu dodać krótkie __delay_ms(5) dla wygładzenia ADC
    }
}
