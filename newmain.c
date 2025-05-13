/* 
 *  File:   main.c
 *  Author: local
 *  169405 Oleksandr Hrypas 
 *  Created on 7?kwietnia?2025, 9:58
 *
 */

//----------------------------------------------------------------------------
//  KONFIGURACJA
//----------------------------------------------------------------------------
#pragma config POSCMOD = NONE       // Primary Oscillator Select (HS Oscillator mode selected)
#pragma config OSCIOFNC = OFF       // Primary Oscillator Output Function (OSC2/CLKO/RC15 functions as CLKO (FOSC/2))
#pragma config FCKSM = CSDCMD       // Clock Switching and Monitor (disabled)
#pragma config FNOSC = FRC          // Oscillator Select (FRC)
#pragma config IESO = OFF           // Internal External Switch Over Mode (disabled)

// CONFIG1
#pragma config WDTPS = PS32768      // Watchdog Timer Postscaler (1:32,768)
#pragma config FWPSA = PR128        // WDT Prescaler (1:128)
#pragma config WINDIS = ON          // Standard Watchdog Timer
#pragma config FWDTEN = OFF         // Watchdog Timer disabled
#pragma config ICS = PGx2           // Comm Channel Select
#pragma config GWRP = OFF           // General Code Segment Write Protect (disabled)
#pragma config GCP = OFF            // General Code Segment Code Protect (disabled)
#pragma config JTAGEN = OFF         // JTAG Port Enable (disabled)


//----------------------------------------------------------------------------
//  BIBLIOTEKI
//----------------------------------------------------------------------------
#include "xc.h"
#include <libpic30.h>
#include <stdint.h>                 
#include <math.h>



//  DEFINICJE I ZMIENNE GLOBALNE
#define BIT_VALUE(value,noBit) (((value) >> (noBit)) & 1)

//Zmienne u?ywane w liczeniu, przechowywaniu stanów, itp.
unsigned  portValue   = 0;  // Bie??ca warto?? wysy?ana na PORTA w trybach bin./Gray.
unsigned  bcdValue    = 0;  // Licznik w systemie BCD (0?99).
uint8_t   snakePos    = 0;      // 0 ? 5
int8_t    snakeStep   = +1;     // kierunek
unsigned  queueMove   = 0;  // Sterowanie efektem ?kolejki?.
unsigned  queueTemp   = 0;  // |
unsigned  tens        = 0;  // |
unsigned  ones        = 0;  // |
unsigned  queueKon    = 0;  // |
unsigned  iq          = 7;  // |

//Zmienne do obs?ugi przycisków
char prevFirst     = 6;     // Detekcja zbocza (0?1) przycisków.
char prevSecond    = 7;     // |
char currentFirst  = 0;     // |
char currentSecond = 0;     // |
char program       = 0;     // Aktualnie wybrany tryb 0?8.

//Zmienne do generatora pseudolosowego 
int val = 1;  // Rejestr i bit sprz??enia generatora pseudolosowego (LFSR 7?bit).
int xor = 0;

//Flaga ustawiana w przerwaniu 
volatile uint8_t tick = 0;  // 1 = czas na wykonanie kolejnego kroku animacji



//  FUNKCJA SPRZ??ENIA
/*  Zwraca XOR wybranych bitów rejestru v wed?ug maski 0b1110011 (tap?y LFSR).
 *  Wynik jest dopisywany jako MSB przy ka?dej iteracji, generuj?c sekwencj?
 *  pseudolosow? (255?elementowy cykl). */
int sprzezenie(unsigned int v)
{
    return  BIT_VALUE(v,0)     // tap 0
          ^ BIT_VALUE(v,1)     // tap 1
          ^ BIT_VALUE(v,2)     // tap 2
          ^ BIT_VALUE(v,4)     // tap 4
          ^ BIT_VALUE(v,5);    // tap 5  
}



void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void)
{
    tick = 1;             // sygna? do p?tli g?ównej
    _T1IF = 0;            // skasowanie flagi przerwania
}


//  FUNKCJE POMOCNICZE DO OBS?UGI PRZYCISKÓW I ZAKRESÓW
void odczytajPrzyciski(void)
{
    // Zapami?tanie stanu przycisków przed krótkim opó?nieniem 
    prevFirst  = PORTDbits.RD6;
    prevSecond = PORTDbits.RD13;
    __delay32(15000);               // eliminacja drga? styków
    currentFirst  = PORTDbits.RD6;
    currentSecond = PORTDbits.RD13;

    // Przyci?ni?cie RD6 ? poprzedni program 
    if ((currentFirst - prevFirst) == -1)  program--;
    // Przyci?ni?cie RD13 ? nast?pny program 
    if ((currentSecond - prevSecond) == -1) program++;
}

// Sprawdzanie numeru programu (cyklicznie 0?8) 
void sprawdzZakresProgramu(void)
{
    if (program > 8) program = 0;
    if (program < 0) program = 8;
}

// Zmienna pomocnicza dla kodu BCD 
void kontrolaBCD(void)
{
    if (bcdValue > 99) bcdValue = 1;
    if (bcdValue == 0) bcdValue = 99;
}

// Kontrola parametrów kolejki 
void kontrolaKolejki(void)
{
    if (queueTemp > 8) queueTemp = 0;
    if (iq == 0)
    {
        iq        = 7;
        queueTemp = 0;
        queueKon  = 0;
    }
}



void wykonajProgram(void)
{
    switch (program)
    {
        // 1. Licznik binarny od 0 do 255 
        case 0:
            LATA = ++portValue;
            break;

        // 2. Licznik binarny od 255 do 0 
        case 1:
            LATA = --portValue;
            break;

        // 3. Licznik w kodzie Graya od 0 do 255 
        case 2:
            portValue++;
            LATA = (portValue >> 1) ^ portValue;
            break;

        // 4. Licznik w kodzie Graya od 255 do 0 
        case 3:
            portValue--;
            LATA = (portValue >> 1) ^ portValue;
            break;

        // 5. Licznik BCD od 0 do 99 
        case 4:
            bcdValue++;
            ones = bcdValue % 10;
            tens = (bcdValue - ones) / 10;
            LATA = (tens << 4) + ones;    // 4 starsze bity = dziesi?tki
            break;

        // 6. Licznik BCD od 99 do 0 
        case 5:
            bcdValue--;
            ones = bcdValue % 10;
            tens = (bcdValue - ones) / 10;
            LATA = (tens << 4) + ones;
            break;

        // 7. W??yk poruszaj?cy si? lewo?prawo (3?bitowy) 
        case 6:
            snakePos += snakeStep;                   // ruch
            if (snakePos == 5 || snakePos == 0)      // odbicie ?od ?ciany?
                snakeStep = -snakeStep;              // natychmiast zmiana kierunku
            LATA = (0x07u << snakePos);
            break;

        // 8. Kolejka 
        case 7:
            queueMove = (1U << queueTemp);
            if (queueTemp == iq)
            {
                queueKon   = 255 - ((1U << iq) - 1);
                iq--;
                queueMove  = 0;
                queueTemp  = 0;
                LATA       = queueKon;
            }
            else
            {
                LATA = queueKon + queueMove;
                queueTemp++;
            }
            break;

        // 9. Generator liczb pseudolosowych w oparciu o 1110011 
        case 8:
            for(int i = 0; i < 5; i++) {
                xor = sprzezenie(val);
                val = val >> 1;
                val += (xor << 7);
                LATA = val;
            }
            break;
    }
}


//  FUNKCJA MAIN
int main(void)
{
    // Ustawienia portów 
    TRISA = 0x0000;    // PORTA jako wyj?cie
    TRISD = 0xFFFF;    // PORTD jako wej?cie

    // Konfiguracja Timera1
    T1CON = 0x8030;    // Timer w??czony, preskaler 1:256
    _T1IE = 1;         // Zezwolenie na przerwanie
    _T1IP = 1;         // Priorytet przerwania
    PR1   = 0x0FFF;    // Okres przerwa?

    while (1)
    {
        // Odczyt i obs?uga przycisków 
        odczytajPrzyciski();
        // Upewnienie si?, ?e program mie?ci si? w zakresie 0..8 
        sprawdzZakresProgramu();
        // Kontrola zakresu licznika BCD 
        kontrolaBCD();
        // Kontrola parametrów kolejki 
        kontrolaKolejki();

        // Je?eli ISR ustawi? flag? ? wykonujemy kolejny krok animacji 
        if (tick)
        {
            tick = 0;
            wykonajProgram();
        }
    }
    return 0;
}
