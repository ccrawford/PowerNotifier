#ifndef HardwareConstants_h
#define HardwareConstants_h

#include "MatrixPanel_CC.h"


#define BUTTON_PIN 4


  // https://chrishewett.com/blog/true-rgb565-colour-picker/

#define COLOR_BLACK 0x0000
#define COLOR_BLUE 0x001F
#define COLOR_LIGHTBLUE 0x0D9F
#define COLOR_RED 0xF800
#define COLOR_DARKORANGE 0xFBA0
#define COLOR_ORANGE 0xFD20
#define COLOR_GREEN 0x07E0
#define COLOR_CYAN 0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW 0xFFE0
#define COLOR_WHITE 0xFFFF
#define COLOR_AL 0xE848
#define COLOR_NL 0x016E
#define COLOR_WHITE80 0xAD55 //AAAAAA
#define COLOR_WHITE50 0x8C51 //888888
#define COLOR_WHITE20 0x2945 //282828
#define COLOR_BIGCLOCK 0x6d55  // Color is kind of a light blue.
#define COLOR_WARNING  0xe429  // use an orange color for warnings.
#define COLOR_ERROR 0x8000  // use red for errors.
#define COLOR_WIN 0x07E0  // use green for wins.
#define COLOR_LOSS 0xF800  // use red for losses.
#define COLOR_TIE 0xf7d9  // real light yellow.
#define COLOR_DAY 0x0FEA2  // use a yellow/orange for day
#define COLOR_NIGHT 0x02b4  // use a blue for night
#define COLOR_FAVORITE 0xf986 // Use a light red for favorite
#define COLOR_FOLLOWED 0x6b5f           // Use a light blue for followed teams.

#define COLOR_PLAYOFF_BRACKET COLOR_WHITE20


#define COLOR_HIGHTEMP COLOR_YELLOW
#define COLOR_LOWTEMP COLOR_LIGHTBLUE

// #define DEBUG_WIRING

#define R1 25
#define G1 26
#define BL1 27
#define R2 21
#define G2 22
#define BL2 23
#ifdef DEBUG_WIRING
  #define CH_A 5
  #define CLK 4
#else
  #define CH_A 12  // The debugger uses 12 and 15. D'oh. 
  #define CLK 15
#endif
#define CH_B 16
#define CH_C 17
#define CH_D 18
#define CH_E -1 // assign to any available pin if using two panels or 64x64 panels with 1/32 scan
#define LAT 32
#define OE 33

/*--------------------- MATRIX PANEL CONFIG -------------------------*/
#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module.
#define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1  // Total number of panels chained one to another


#endif