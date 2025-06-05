/*
 * File:   main.c
 * Author: local
 * 169405 Oleksandr Hrypas 
 * Created on 7 kwietnia 2025, 9:58
 */

#pragma config POSCMOD  = XT
#pragma config OSCIOFNC = ON
#pragma config FCKSM    = CSDCMD
#pragma config FNOSC    = PRI
#pragma config IESO     = ON
#pragma config WDTPS    = PS32768
#pragma config FWPSA    = PR128
#pragma config WINDIS   = ON
#pragma config FWDTEN   = ON
#pragma config ICS      = PGx2
#pragma config GWRP     = OFF
#pragma config GCP      = OFF
#pragma config JTAGEN   = OFF

#include <xc.h>
#define  FCY  4000000UL
#include <libpic30.h>

#include <stdio.h>
#include <string.h>
#include "adc.h"
#include "lcd.h"

#define PIN_BTN1 6          // RD6
#define PIN_BTN2 13         // RD13

volatile unsigned int  czas1 = 0, czas2 = 0;   // sekundy pozostałe
volatile unsigned char aktywny    = 1;         // 1 - gracz1, 2 - gracz2
volatile unsigned char odswiez    = 0;         // flaga LCD
volatile unsigned char koniecCzasu= 0;         // sygnał TIME OUT
volatile unsigned char przegrany  = 0;         // numer gracza

// --- Debouncing
volatile uint8_t debounce_btn1 = 0;
volatile uint8_t debounce_btn2 = 0;
#define DEBOUNCE_TIME 5   // ok. 5*1s=5s dla Timer1 1Hz, jeśli Timer1 szybciej to np. 5*100ms

static void wyswietlCzas(void)
{
    char linia[17];
    LCD_ClearScreen();
    sprintf(linia, "P1 %02u:%02u", czas1/60, czas1%60);
    LCD_PutString(linia, strlen(linia));
    LCD_PutChar('\n');
    sprintf(linia, "P2 %02u:%02u", czas2/60, czas2%60);
    LCD_PutString(linia, strlen(linia));
}

static void przegrana(unsigned char kto)
{
    LCD_ClearScreen();
    if (kto == 1) LCD_PutString("P1 PRZEGRAL!", 13);
    else          LCD_PutString("P2 PRZEGRAL!", 13);
    while (1);
}

static void startTimer(void)
{
    T1CON = 0;
    T1CONbits.TCKPS = 3;                 // 1:256
    PR1 = (FCY/256/10) - 1;              // co 0.1 sekundy (10 Hz)
    IPC0bits.T1IP = 4;
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
    T1CONbits.TON = 1;
}

static void startPrzyciski(void)
{
    TRISDbits.TRISD6  = 1;
    TRISDbits.TRISD13 = 1;

    CNPU1bits.CN15PUE = 1;               // pull-up RD6
    CNPU2bits.CN19PUE = 1;               // pull-up RD13

    CNEN1bits.CN15IE = 1;
    CNEN2bits.CN19IE = 1;

    IPC4bits.CNIP = 5;
    IFS1bits.CNIF = 0;
    IEC1bits.CNIE = 1;
}

static unsigned int pobierzMinutyStart(void)
{
    unsigned int a = ADC_Read10bit(ADC_CHANNEL_POTENTIOMETER);
    return (a < 341) ? 1 : (a < 682) ? 3 : 5;
}

volatile uint8_t tick_100ms = 0;
volatile uint8_t tsec = 0;

void __attribute__((interrupt, auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF = 0;

    // --- Debouncing: zmniejszaj licznik co 0.1s
    if (debounce_btn1 > 0) debounce_btn1--;
    if (debounce_btn2 > 0) debounce_btn2--;

    // --- Zliczanie sekund do odjęcia czasu co 1 sekundę
    if (++tsec >= 10) {
        tsec = 0;
        // Odejmowanie czasu tylko co sekundę!
        if (aktywny == 1)
        {
            if (czas1) --czas1; else { przegrany = 1; koniecCzasu = 1; return; }
        }
        else
        {
            if (czas2) --czas2; else { przegrany = 2; koniecCzasu = 1; return; }
        }
        odswiez = 1;
    }
}

void __attribute__((interrupt, auto_psv)) _CNInterrupt(void)
{
    volatile uint16_t tmp = PORTD;
    IFS1bits.CNIF = 0;

    // Przyciski obsługiwane z debouncingiem
    if (!PORTDbits.RD6 && debounce_btn1 == 0) {  // BTN1 naciśnięty, gracz1
        if (aktywny != 2) {
            aktywny = 2;   // przełącz na gracza 2
            debounce_btn1 = DEBOUNCE_TIME;
        }
    }
    else if (!PORTDbits.RD13 && debounce_btn2 == 0) { // BTN2 naciśnięty, gracz2
        if (aktywny != 1) {
            aktywny = 1;   // przełącz na gracza 1
            debounce_btn2 = DEBOUNCE_TIME;
        }
    }
}

int main(void)
{
    AD1PCFG = 0xFFFB;                       // AN0 analog reszta cyfrowo

    LCD_Initialize();
    ADC_SetConfiguration(ADC_CONFIGURATION_DEFAULT);
    ADC_ChannelEnable(ADC_CHANNEL_POTENTIOMETER);

    startPrzyciski();
    startTimer();

    unsigned int startMinuty = pobierzMinutyStart();
    czas1 = czas2 = startMinuty * 60U;

    wyswietlCzas();

    while (1)
    {
        if (koniecCzasu)
            przegrana(przegrany);

        if (odswiez)
        {
            odswiez = 0;
            wyswietlCzas();
        }
    }
    return 0;
}
