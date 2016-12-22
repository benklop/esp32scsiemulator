#include "config.h"
#include <SPI.h>  // Include SPI if you're using SPI
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include "LUNS.h"
#include "svcScreen.h"
#include "svcTarget.h"

//////////////////////////
// MicroOLED Definition //
//////////////////////////
#define OLED_RESET 27 // Connect RST to pin 9 (SPI & I2C)
#define OLED_DC    28 // Connect DC to pin 8 (SPI only)
#define OLED_CS    31 // Connect CS to pin 10 (SPI only)

//////////////////////////////////
// MicroOLED Object Declaration //
//////////////////////////////////
MicroOLED oled(OLED_RESET, OLED_DC, OLED_CS);  // SPI Example

void screenInit() {
  oled.begin();     // Initialize the OLED
  oled.clear(ALL);  // Clear the library's display buffer
  oled.display();   // Display what's in the buffer (splashscreen)

  oled.clear(PAGE);
  oled.setFontType(0);
  oled.setCursor(0,0);

  oled.print("TSE");
}

void screenStatus(const char *st) {
  char sb[11];
  strncpy(sb, st, 10);
  sb[10] = 0;
  int w = strlen(st);
  if(w > 10) w = 10;
  int x = (oled.getLCDWidth() - (w * 6)) / 2;
  int y = (oled.getLCDHeight() / 2) - 4;

  oled.setCursor(x, y);
  oled.print(sb);
  oled.display();
  oled.rectFill(0, 8, oled.getLCDWidth(), oled.getLCDHeight()-8, BLACK, NORM);
}

void screenCardSize(uint32_t blocks) {
  char strSize[16];
  oled.setCursor(oled.getLCDWidth() - (7*6), 0);
  sprintf(strSize, "%4lu GB", blocks / 2000000);
  oled.print(strSize);
  oled.display();
}

void screenRefresh() {
  int sID = target_GetID();
  for(int p = 0; p < 8; p++) {
    int pa = 0;
    int pe = sID & (1<<p);
    for(int ii=p*8;ii<((p+1)*8);ii++) {
      pa |= lun[ii].Active;
      lun[ii].Active = 0;
    }
    // Paint Active Devices
    oled.rectFill(p * 8, oled.getLCDHeight() - 4, 8, 2, pa ? WHITE : BLACK, NORM);
    // Paint Enabled Devices
    oled.rectFill(p * 8, oled.getLCDHeight() - 2, 8, 2, pe ? WHITE : BLACK, NORM);
  }
  oled.display();
}
