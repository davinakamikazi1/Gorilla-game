﻿#ifndef LCD_H
#define LCD_H


//
// LCD Interface
//
// This module initializes LCD hardware, blanks the shadow memory then displays it on
// the screen.
//
//
// initialize LCD - Call this once at the beginning of time.
// It sets up LCD hardware, blanks the shadow memory then displays it on
// the screen.
//




void init_lcd(void);
//
// Copy shadow memory to LCD screen.
//
void refresh_screen(void);
//
// Clear the shadow memory.
//
void blank_screen(void);
//
// Shadow memory. 1024 bytes. Eight 128-byte pages. Each page corresponds
// to 8 rows of pixels. screen[0] is upper left, screen[127] is upper right,
// screen[1023] is lower right. Least significant bit of each byte is on the
// top pixel row of its page.
//
extern xdata unsigned char screen[1024];         
//
// Handy 5x7 font that will come in handy in later labs. Always put at least
// a one pixel space between characters.
//
extern const unsigned char code font5x8[]; // Declare font array  


#endif /* LCD_H */
