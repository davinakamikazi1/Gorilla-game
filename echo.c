#include <C8051F020.h>
#include "lcd.h"






void main() {
    // Disable watchdog
    WDTCN = 0xDE;
    WDTCN = 0xAD;
    // Enable port output + crossbar
    XBR2 = 0x40;  // Enable crossbar
    XBR0 = 0x04;  // Enable UART0 (optional)
    // Start external oscillator
    OSCXCN = 0x67;  // Crystal oscillator mode
    TMOD = 0x20;    // Timer1 Mode 2 (auto-reload)
    TH1 = -167;     // Delay ~1 ms
    TR1 = 1;
    while (!TF1);   // Wait 1 ms
    TR1 = 0;
    TF1 = 0;
    // Wait for oscillator to stabilize
    while (!(OSCXCN & 0x80));
    OSCICN = 0x08;  // Switch to external crystal (22.1184 MHz)
    init_lcd();     // Initialize LCD and clear screen
    
    // Clear the screen properly (remove cross-hatch pattern)
    clear_screen();
        //srand(read_adc(0)); // Seed RNG for random skyline/wind  
    
    game_loop();    // Start the Gorilla game!
}
