﻿#include <C8051F020.h>
#include "lcd.h"
#include <stdlib.h>


sbit LAUNCH_BUTTON = P3^6;
sbit RESET_BUTTON = P3^7;
sbit DIP1 = P1^0;
sbit DIP2 = P1^1;
sbit Speaker = P1^6;




xdata unsigned char skyline[128];
xdata signed char wind;
xdata unsigned char speed, angle;
xdata unsigned char current_player;
bit game_over;


#define GRAVITY 9.8
#define WIND_FACTOR 0.01f
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SPRITE_WIDTH  8
#define SPRITE_HEIGHT 8


bit positionUpdateFlag = 0;
int time = 0;
typedef struct { int x, y, vx, vy; } Banana;
typedef struct { unsigned char x, y; } Position;


xdata Position gorilla1, gorilla2;






code int banana[4] = {0x6, 0xF, 0x9, 0x0};


void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for (i = 0; i < ms; i++) for (j = 0; j < 1275; j++);
}


void wait_launch_button() {
    while (LAUNCH_BUTTON); delay_ms(10);
    while (!LAUNCH_BUTTON); delay_ms(10);
}


void wait_reset_button() {
    while (RESET_BUTTON); delay_ms(10);
    while (!RESET_BUTTON); delay_ms(10);
}




void adc_init(void) {  
    ADC0CN = 0x80;      // Enable ADC  
    REF0CN = 0x03;      // Enable internal reference  
    ADC0CF = 0x40;      // SAR clock = 2.5MHz  
}  
  
// ADC read function  
unsigned char read_adc(unsigned char channel) {
    unsigned int i;          


    //  select the input
    AMX0SL = channel;


    //  small settle delay (~2 µs)
    for (i = 0; i < 20; i++) {
        ;                   // empty loop body
    }


    // 3) start conversion
    ADC0CN &= ~0x20;        //  flag
    ADC0CN |=  0x10;        // start conversion
    while (!(ADC0CN & 0x20)) {
        ;                   
    }


    // return the low byte for full 0-255 range
    return ADC0L;
}


    
unsigned char read_speed(void) {  
    unsigned char raw = read_adc(0);  
    return (raw * 100) / 255;  // Scale to 0-100  
}  
  
unsigned char read_angle(void) {  
    unsigned char raw = read_adc(1);  
    return (raw * 90) / 255;   // Scale to 0-90 degrees  
} 




void clear_screen() {
    int i;
    for (i = 0; i < 1024; i++) screen[i] = 0;
}
//draw a text
void draw_text(unsigned char x, unsigned char y, const char* s) {
    int i, j, c;
    unsigned int addr = y/8 * 128 + x;
    for (i = 0; s[i] != 0; i++) {
        c = s[i] - 32;
        for (j = 0; j < 5; j++) screen[addr++] = font5x8[c * 5 + j];
        screen[addr++] = 0;
    }
}
//integer value into a string 
void itoa(int val, char* buf) {
    char temp[6];
    int i = 0, j, is_negative = 0;
    if (val < 0) { is_negative = 1; val = -val; }
    do {
        temp[i++] = (val % 10) + '0';
        val /= 10;
    } while (val && i < 5);
    if (is_negative) temp[i++] = '-';
    for (j = 0; j < i; j++) buf[j] = temp[i - j - 1];
    buf[j] = '\0';
}
//draw number
void draw_number(unsigned char x, unsigned char y, unsigned char value) {
    char buf[6];
    itoa(value, buf);
    draw_text(x, y, buf);
}
code unsigned char sine[] = { 48, 89, 116, 126, 116, 89, 48, 0, -48, -89, -116, -126, -116, -89, -48, 0 };
unsigned int sound_envelope = 512;
unsigned char phase = 0; // current point in sine to output
unsigned int duration = 0; // number of cycles left to output


int RCAPcalc(int freq) {
    int overflow = freq * 16;
    int RCAP = -22118400 / overflow;
    return RCAP;
}


void timer2(void) interrupt 5 {
    DAC0H = ((sine[phase]*(long)sound_envelope>>10)+128);
    
    TF2 = 0;
    if (phase < sizeof(sine)-1) // if mid-cycle
    { // complete it
        phase++;
    }
    else if (duration > 0) // if more cycles left to go
    { // start a new cycle
        phase = 0;
        duration--;
        if(sound_envelope > 0){
            sound_envelope--;
        }
    }
}
void wait_sound() {
    EA = 0;
    while(duration) {
        EA = 1;
        delay_ms(1); // Use your existing delay function
        EA = 0;
    }
    EA = 1;
}
void explosion_sound() {
    sound_envelope = 512;
    // Lower frequency for explosion
    RCAP2L = RCAPcalc(300);
    RCAP2H = RCAPcalc(300) >> 8;
    duration = 20; // Longer duration
    wait_sound();
}


// using sine wave approach
void launch_sound() {
    sound_envelope = 512;
    // Higher frequency for launch
    RCAP2L = RCAPcalc(800);
    RCAP2H = RCAPcalc(800) >> 8;
    duration = 20;
    wait_sound();
}


void sound_init() {
    
    T2CON = 4; // timer 2, auto reload
    RCAP2L = 0x40; // set up for 800Hz initially
    RCAP2H = 0xF9;
    
    // Set up DAC for sound output
    REF0CN = 3; // turn on voltage reference
    DAC0CN = 0x9C; // update on timer 2, left justified
    
    // Enable Timer 2 interrupt
    ET2 = 1; // Set in IE register
    EA = 1;  // Global interrupt enable
}


void draw_explosion(int x, int y) {
    int i, j;
    for (i = -2; i <= 2; i++) {
        for (j = -2; j <= 2; j++) {
            if (x+i >= 0 && x+i < 128 && y+j >= 0 && y+j < 64) {
                screen[(y+j)/8 * 128 + (x+i)] |= (1 << ((y+j)%8));
            }
        }
    }
}


int draw_banana(int x, int y) {
    int mask;
    int page = y >> 3;
    int shift = y & 7;
    int i = page * 128 + x;
    int hit = 0;
    char k;


    if (x < -4 || x > 131 || y < -4 || y > 67) {
        return 0;
    }


    for (k=0; k<3; k++) {
        if (x+k >= 0 && x+k < 128) {
            mask = banana[k] << shift;
            if (y >= 0 && y < 64) {
                hit |= screen[i+k] & mask;
            }
            if (y > -8 && y < 56) {
                hit |= screen[i+k+128] & (mask >> 8);
            }
        }
    }


    if (!hit) {
        for (k=0; k<3; k++) {
            if (x+k >= 0 && x+k < 128) {
                mask = banana[k] << shift;
                if (y >= 0 && y < 64) {
                    screen[i+k] |= mask;
                }
                if (y > -8 && y < 56) {
                    screen[i+k+128] |= (mask >> 8);
                }
            }
        }
    }


    return hit;
}


code unsigned char gorillaSprite1[8] = { 0x3C, 0x7E, 0xFF, 0x7E, 0x3C, 0x3C, 0x24, 0x42 };


code unsigned char gorillaSprite2[8] = { 0x3C, 0x7E, 0xFF, 0x7E, 0x3C, 0x3C, 0x24, 0x42 };


void set_pixel(unsigned char x, unsigned char y) { 
        unsigned int byte_index;
        unsigned char bit_mask;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return; // Out of bounds  
  
    byte_index = (y / 8) * SCREEN_WIDTH + x;  
    bit_mask = 1 << (y % 8);  
  
    screen[byte_index] |= bit_mask;  
}  
void draw_sprite(unsigned char x, unsigned char y, const unsigned char *sprite) {  
    int row;
        int col;
        unsigned char row_data;
        for (row = 0; row < 8; row++) {  
        row_data = sprite[row];  
        for ( col = 0; col < 8; col++) {  
            // Check if the bit for this column is set  
            if (row_data & (0x80 >> col)) {  
                set_pixel(x + col, y + row); // Draw pixel if bit is set  
            }  
        }  
    }  
        }


void draw_gorilla(Position* gorilla, unsigned char player) {
    if (player == 1) {
        draw_sprite(gorilla->x, gorilla->y, gorillaSprite1);
    } else {
        draw_sprite(gorilla->x, gorilla->y, gorillaSprite2);
    }
}


void draw_scene() {
    int x, y;
    clear_screen();
    for (x = 0; x < 128; x++) {
        unsigned char h = skyline[x];
        for (y = 63; y >= 64 - h; y--) {
            screen[(y / 8) * 128 + x] |= (1 << (y % 8));
        }
    }
    draw_gorilla(&gorilla1, 1); // Player 1
    draw_gorilla(&gorilla2, 2); // Player 2
}


void draw_wind() {
    char wind_val[6];
    int x = 100, y = 2, i;
    itoa(wind, wind_val);
    draw_text(110, 0, wind_val);
    if (wind > 0) for (i = 0; i < wind; i++) screen[(y / 8) * 128 + x + i] |= (1 << (y % 8));
    else if (wind < 0) for (i = 0; i > wind; i--) screen[(y / 8) * 128 + x + i] |= (1 << (y % 8));
}




void erode_skyline(int x, int y) {
    // Random chance of erosion (50%)
    if (rand() % 2 == 0) {
        int i, pos_x;
        for (i = -2; i <= 2; i++) {
            pos_x = x + i;
            if (pos_x >= 0 && pos_x < 128 && y >= 64 - skyline[pos_x]) {
                skyline[pos_x] = (skyline[pos_x] > 5) ? skyline[pos_x] - 5 : 0;
            }
        }
    }
}


void randomize_skyline() {
    int x = 0;
    while (x < 128) {
        unsigned char height = 15 + rand() % 30;
        unsigned char width = 6 + rand() % 4;
        int end = x + width;
        if (end > 128) end = 128;
        while (x < end) skyline[x++] = height;
    }
}


void randomize_wind() { wind = (rand() % 9) - 4; }


void place_gorillas(void) {
    int  x;
    int  best_x1 = 0;
    unsigned char best_h1 = 0;
    int  best_x2 = SCREEN_WIDTH/2;
    unsigned char best_h2 = 0;


    //  player 1: find the tallest building in the left half
    for (x = 0; x < SCREEN_WIDTH/2; x++) {
        if (skyline[x] > best_h1) {
            best_h1 = skyline[x];
            best_x1 = x;
        }
    }
    gorilla1.x = best_x1;
    gorilla1.y = SCREEN_HEIGHT - best_h1 - SPRITE_HEIGHT;


    // player 2: find the tallest building in the right half
    for (x = SCREEN_WIDTH/2; x < SCREEN_WIDTH; x++) {
        if (skyline[x] > best_h2) {
            best_h2 = skyline[x];
            best_x2 = x;
        }
    }
    gorilla2.x = best_x2;
    gorilla2.y = SCREEN_HEIGHT - best_h2 - SPRITE_HEIGHT;
}




// Integer values scaled by 1000 for precision
code int sin_table[91] = {
      0,   17,   35,   52,   70,   87,  105,  122,  139,  156,
    174,  191,  208,  225,  242,  259,  276,  292,  309,  326,
    342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
    500,  515,  530,  545,  559,  574,  588,  602,  616,  629,
    643,  656,  669,  682,  694,  707,  719,  731,  743,  755,
    766,  777,  788,  798,  809,  819,  829,  838,  848,  857,
    866,  874,  883,  891,  899,  906,  914,  921,  927,  934,
    940,  946,  951,  957,  962,  966,  971,  975,  979,  982,
    985,  988,  991,  994,  996,  998,  999, 1000, 1000, 1000,
   1000
};


code int cos_table[91] = {
   1000, 1000, 1000,  999,  998,  996,  994,  991,  988,  985,
    982,  979,  975,  971,  966,  962,  957,  951,  946,  940,
    934,  927,  921,  914,  906,  899,  891,  883,  874,  866,
    857,  848,  838,  829,  819,  809,  798,  788,  777,  766,
    755,  743,  731,  719,  707,  694,  682,  669,  656,  643,
    629,  616,  602,  588,  574,  559,  545,  530,  515,  500,
    485,  469,  454,  438,  423,  407,  391,  375,  358,  342,
    326,  309,  292,  276,  259,  242,  225,  208,  191,  174,
    156,  139,  122,  105,   87,   70,   52,   35,   17,    0,
      0
};




void simulate_banana(Position p,
                    unsigned char angle,
                    unsigned char speed,
                    char           player,
                    int           *hit_x,
                    int           *hit_y)
{
    // 1) Declarations up front
    const float dt     = 0.15f;
    float       t      = 0.0f;
    float       vx0    = ((float)speed * cos_table[angle]) / 1000.0f;
    float       vy0    = ((float)speed * sin_table[angle]) / 1000.0f;
    float       pos_x, pos_y;
    int         x, y;
    char        hit;


    //  Flip horizontal for player 2
    if (player == 2) {
        vx0 = -vx0;
    }


    //  Main animation loop
    while (1) {
        t += dt;


        // compute new position
        pos_x = (float)p.x + SPRITE_WIDTH/2 + vx0*t
              + 0.5f * wind * WIND_FACTOR * t * t;
        pos_y = (float)p.y             - vy0 * t
              + 0.5f * GRAVITY        * t * t;


        x = (int)pos_x;
        y = (int)pos_y;


        // off left/right?
        if (x < 0 || x >= SCREEN_WIDTH) {
            return;
        }


        // redraw background & wind
        draw_scene();
        draw_wind();


        // only draw & test collisions if on-screen vertically
        if (y >= 0 && y < SCREEN_HEIGHT) {
            hit = draw_banana(x, y);
            refresh_screen();


            // gorilla collision?
            if (hit &&
               ((player == 1 &&
                 x >= gorilla2.x &&
                 x <  gorilla2.x + SPRITE_WIDTH &&
                 y >= gorilla2.y &&
                 y <  gorilla2.y + SPRITE_HEIGHT)
                ||
                (player == 2 &&
                 x >= gorilla1.x &&
                 x <  gorilla1.x + SPRITE_WIDTH &&
                 y >= gorilla1.y &&
                 y <  gorilla1.y + SPRITE_HEIGHT)))
            {
                *hit_x    = x;
                *hit_y    = y;
                game_over = 1;
                return;
            }


            // building collision?
            if (y >= SCREEN_HEIGHT - skyline[x]) {
                *hit_x = x;
                *hit_y = y;
                return;
            }
        }
        else {
            // banana off top/bottom, just clear the last frame
            refresh_screen();
        }


        delay_ms(30);


        // bail if it falls way below screen
        if (y > SCREEN_HEIGHT + 32) {
            return;
        }
    }
}






void blank_area(unsigned char x, unsigned char y, unsigned char width, unsigned char height) {  
    unsigned int addr = (y/8) * 128 + x;  
    unsigned char i;  
      
    for (i = 0; i < width; i++) {  
        screen[addr + i] = 0x00;  
    }  
}


// Draw Player X Wins! and wait for the Launch button before returning.


void drawWin(char winner) {
    //  Clear the display
    clear_screen();


    //  Show which player won, centered roughly on the screen
    if (winner == 1) {
        draw_text(24, 28, "Player 1 Wins!");
    } else {
        draw_text(24, 28, "Player 2 Wins!");
    }


    //  Prompt to continue
    draw_text(16, 36, "Press Launch to cont.");
    refresh_screen();


    //  Wait for Launch press & release (debounce)
    delay_ms(20);
    while (!LAUNCH_BUTTON);  // wait until button down
    delay_ms(20);
    while (LAUNCH_BUTTON);   // wait until button up
    delay_ms(20);
}




void run_turn(char player) {  
    Position shooter;  
    int hit_x, hit_y;  
    unsigned char raw_speed, raw_angle;  
    unsigned char speed, angle;  
    unsigned char prev_speed = 255;  // Force first display  
    unsigned char prev_angle = 255;  
  
    // Set shooter position  
    shooter = (player == 1) ? gorilla1 : gorilla2;  
  
    // Clear screen and draw buildings/gorillas  
    clear_screen();  
    draw_scene();
        draw_wind();
        draw_text(0, 0, (player == 1) ? "P1" : "P2");  
    draw_text(20, 0, "Speed:");  
    draw_number(60, 0, speed);  
    draw_text(20, 10, "Angle:");  
    draw_number(60, 10, angle);  
    draw_wind();  
    refresh_screen(); 
  
    // Adjustment loop: let player set speed and angle  
    while (LAUNCH_BUTTON) {  
        // Read raw potentiometer values  
        raw_speed = read_adc(0);  // AIN0  
        raw_angle = read_adc(1);  // AIN1  
  
        // Scale for gameplay  
        speed = (raw_speed * 100) / 255;   // 0–100  
        angle = (raw_angle * 90) / 255;    // 0–90  


        // Only update display if values changed  
        if (speed != prev_speed) {
            blank_area(60, 0, 30, 8);
            draw_number(60, 0,speed);
            prev_speed = speed;
        }


        if (angle != prev_angle) {
            blank_area(60, 10, 30, 8);
            draw_number(60, 10, angle);
            prev_angle = angle;
        }   
                  refresh_screen();
        delay_ms(20);  // Flicker prevention  
    }  
  
    // Wait for button release (debounce)  
    delay_ms(20);  
    while (!LAUNCH_BUTTON);  
    delay_ms(20);  
  
    // Launch banana  
    launch_sound(); 
        draw_text(0, 50, "Firing...");
    refresh_screen();
        launch_sound();
    simulate_banana(shooter, angle, speed, player, &hit_x, &hit_y);  
  
    // If game not over, show explosion  
        explosion_sound();
    draw_explosion(hit_x, hit_y);  
    erode_skyline(hit_x, hit_y);  
    refresh_screen();  
    delay_ms(1000);  
            if (game_over) {
        // current player just scored
        drawWin(player);
    }
}
 


void initialize_game(void) {
    //  Reset flags and turn
    game_over     = 0;
    current_player = 1;     // Player 1 always starts


    //  New world
    randomize_skyline();
    randomize_wind();
    place_gorillas();


    //  Draw initial screen
    clear_screen();
    draw_scene();   // skyline + gorillas
    draw_wind();    // wind bar + number
    refresh_screen();


    //  Give a moment so the player sees the new skyline & positions
    delay_ms(500);
}




void draw_game_over() {
    clear_screen();
    draw_text(30, 28, "Game Over");
    draw_text(8, 36, "Press Launch to Restart");
    refresh_screen();
    wait_launch_button();  
        initialize_game();
}
 


/*void test_live_pots_lab07(void) {  
    unsigned char speed, angle;  
    char buf[6];  
  
    adc_init(); // Initialize ADC once  
  
    blank_screen();  
    draw_text(0, 0, "LAB07 Pot Test");  
  
    while (1) {  
        speed = read_adc(0);  // AIN0  
        angle = read_adc(1);  // AIN1  
  
        // Display speed  
        itoa(speed, buf);  
        blank_area(0, 10, 128, 8);  
        draw_text(0, 10, "Speed:");  
        draw_text(50, 10, buf);  
  
        // Display angle  
        itoa(angle, buf);  
        blank_area(0, 20, 128, 8);  
        draw_text(0, 20, "Angle:");  
        draw_text(50, 20, buf);  
  
        refresh_screen();  
        delay_ms(100);  
    }  
} */ 


void game_loop() {
    adc_init();                // ADC for pots
    P1MDOUT |= 0x40;           // P1.6 (SPEAKER) 
    XBR2    = 0x40;            // enable crossbar so SPEAKER drives
    sound_init();
    EA      = 1;               // global interrupts on
    while (1) {
        clear_screen();
        draw_text(50, 28, "Ready");
        draw_text(10, 36, "Press Launch Button to Start");
        refresh_screen();
        
        wait_launch_button();
        
        initialize_game();


        
        // Main game loop
        while (!game_over) {
            // Clear status area
            run_turn(current_player);
            
            // Switch players if game isn't over
            if (!game_over) {
                current_player = (current_player == 1) ? 2 : 1;
            }
        }
        
        // Game over screen
        clear_screen();
        draw_text(32, 28, "Game Over!");
        draw_text(8, 44, "Press Launch to Restart");
        refresh_screen();
        
        wait_launch_button();
    }
}
