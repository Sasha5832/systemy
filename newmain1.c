/*
 * File:   main.c
 * Author: local
 * 169405 Oleksandr Hrypas 
 * Created on 7 kwietnia 2025, 9:58
 */

//----------------------------------------------------------------------------
// KONFIGURACJA
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


// BIBLIOTEKI

#include "xc.h"
#include <libpic30.h>
#include <math.h>


// DEFINICJE I ZMIENNE GLOBALNE

#define BIT_VALUE(value,noBit) ((value >> (noBit)) & 1)

// Zmienne u?ywane w liczeniu, przechowywaniu stan?w, itp.
unsigned  portValue    = 0; //Bie??ca warto?? wysy?ana na?PORTA w trybach bin./Gray.
unsigned  bcdValue     = 0; //Licznik w systemie BCD (0?99).
unsigned  snakeMove    = 0; //Pozycja i kierunek 3?bitowego ?w??yka?.
unsigned  snakeDir     = 1; //|
unsigned  queueMove    = 0; // Sterowanie efektem ?kolejki?.
unsigned  queueTemp  = 0;   //|
unsigned  tens         = 0; //|
unsigned  ones         = 0; //|
unsigned  queueKon     = 0; //|
unsigned  iq           = 7; //|


// Zmienne do obs?ugi przycisk?w
char prevFirst     = 6; //Detekcja zbocza (0???1) przycisk?w.
char prevSecond     = 7; //|
char currentFirst  = 0; //|
char currentSecond  = 0; //|
char program    = 0; //Aktualnie wybrany tryb 0?8.

// Zmienne do generatora pseudolosowego
int val = 1; //Rejestr i bit sprz??enia generatora pseudolosowego?(LFSR?7?bit).
int xor = 0; //|

// FUNKCJA SPRZ??ENIA 
// Zwraca XOR?wybranych bit?w rejestru v wed?ug maski 0b1110011 (tap?y?LFSR). 
// Wynik jest dopisywany jako?MSB przy ka?dej iteracji, generuj?c sekwencj? pseudolosow??(255?? cykli).
int sprzezenie(unsigned int v) {
    // Sprz??enie w oparciu o 1110011
    return BIT_VALUE(v,0)
         ^ BIT_VALUE(v,1)
         ^ BIT_VALUE(v,2)
         ^ BIT_VALUE(v,5)
         ^ BIT_VALUE(v,6);
}

// FUNKCJA PRZERWANIA TIMERA1
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void) {
    
    switch (program) {

        // 1. Licznik binarny od 0 do 255
        case 0:
            portValue++;
            LATA = portValue;
            break;

        // 2. Licznik binarny od 255 do 0
        case 1:
            portValue--;
            LATA = portValue;
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
            LATA = (tens * pow(2,4)) + ones;
            break;

        // 6. Licznik BCD od 99 do 0
        case 5:
            bcdValue--;
            ones = bcdValue % 10;
            tens = (bcdValue - ones) / 10;
            LATA = (tens * pow(2,4)) + ones;
            break;

        // 7. W??yk poruszaj?cy si? lewo-prawo (3-bitowy)
        case 6:
            if (snakeDir == 1) {
                if (snakeMove >= 5) {
                   snakeDir = 0;
                } else {
                    snakeMove++;
                }
            } else {
                if (snakeMove == 0) {
                    snakeDir = 1;
                } else {
                    snakeMove--;
                }
            }
            LATA = (0x07 << snakeMove);
            break;


        // 8. Kolejka
        case 7:
            queueMove = 1 * pow(2, queueTemp);
            if (queueTemp == iq) {
                queueKon = 255 - (pow(2, iq) - 1);
                iq--;
                queueMove    = 0;
                queueTemp  = 0;
                LATA         = queueKon;
            }
            else {
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
    
    _T1IF = 0;  // skasowanie flagi przerwania
}


// FUNKCJE POMOCNICZE DO OBS?UGI PRZYCISK?W I ZAKRES?W
void odczytajPrzyciski(void) {
    // Zapami?tanie stanu przycisk?w przed kr?tkim op??nieniem
    prevFirst = PORTDbits.RD6;
    prevSecond = PORTDbits.RD13;
    __delay32(15000);     // eliminacja drga? styk?w
    //for (volatile long i = 0; i < 50000; i++);
    currentFirst = PORTDbits.RD6;
    currentSecond = PORTDbits.RD13;

    // Przyci?ni?cie RD6 -> poprzedni program
    if ((currentFirst - prevFirst) == -1) {
        program--;
    }
    // Przyci?ni?cie RD7 -> nast?pny program
    if ((currentSecond - prevSecond) == -1) {
        program++;
    }
}
// Sprawdzanie numeru programu
void sprawdzZakresProgramu(void) {
    if (program > 8) program = 0;
    if (program < 0) program = 8;
}
// Zmienna pomocnicza dla kodu BCD
void kontrolaBCD(void) {
    if (bcdValue > 99) bcdValue = 1;
    if (bcdValue == 0) bcdValue = 99;
}
// Kontrola parametr?w kolejki
void kontrolaKolejki(void) {
    if (queueTemp > 8) queueTemp = 0;
    if (iq == 0) {
        iq          = 7;
        queueTemp = 0;
        queueKon    = 0;
    }
}

// FUNKCJA MAIN
int main(void) {
    
    // Ustawienia port?w
    TRISA = 0x0000; // PORTA jako wyj?cie
    TRISD = 0xFFFF; // PORTD jako wej?cie
    
    // Konfiguracja Timera1
    T1CON  = 0x8030; // Timer w??czony, preskaler 1:256
    _T1IE  = 1;      // Zezwolenie na przerwanie
    _T1IP  = 1;      // Priorytet przerwania
    PR1    = 0x0FFF; // Okres przerwa?
    
    while(1) {
        // Odczyt i obs?uga przycisk?w
        odczytajPrzyciski();
        // Upewnienie si?, ?e program mie?ci si? w zakresie 0..8
        sprawdzZakresProgramu();
        // Kontrola zakresu licznika BCD
        kontrolaBCD();
        // Kontrola parametr?w kolejki
        kontrolaKolejki();
    }
    
    return 0;
}