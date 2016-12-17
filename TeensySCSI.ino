#include "DebugConsole.h"
#include <Cmd.h>
#include "sdhc.h"
#include <SPI.h>  // Include SPI if you're using SPI
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include <EtherRaw.h> // Include Raw Ethernet library
#include <EtherRawSocket.h>
#include "ff.h"
#include "cmdDispatch.h"
#include "ParseMBR.h"
#include "LUNS.h"
#include "NCR5380.h"

int ncr_dmamode = DMA_REAL;
int phasedelay = 1;
int verbosity = DBG_INFO | DBG_WARN | DBG_ERROR | DBG_GENERAL;
extern int target_interrupt;

extern SD_CARD_DESCRIPTOR sdCardDesc;

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

void setup() {
  int ii;
  uint8_t sID;
  char strSize[16];

#ifdef DEBUG
//  while(!Serial);
#endif

  // Initialize OLED screen
  screenInit();

  // Initialize SCSI interface
  ncr_Init();

  // Initialize Ethernet chip
  etherInit();

  oled.clear(ALL);
  oled.setCursor(0, 0);
  oled.print("TSE");

  screenStatus("SDHC");

  // Initialize SDHC Driver
  DSTATUS stat = SDHC_InitCard();

  oled.setCursor(oled.getLCDWidth() - (7*6), 0);
  sprintf(strSize, "%4d GB", sdCardDesc.numBlocks / 2000000);
  oled.print(strSize);
  oled.display();

  // Initialize our command executive
  screenStatus("EXEC");
  cmdInit();
  dispatchInit();
  execInit();

  screenStatus("LUNS");
  memset(lun, 0, sizeof(lun));
  // Enable default LUNs
  for(ii=0;ii<MAXLUNS;ii+=8) {
    lun[ii].Enabled = 1;
    lun[ii].Type = LUN_DISK;
  }

  screenStatus("CONFIG");
  execHandler((char*)"config.tse");

  // Turn off LUNs for IDs we aren't listening to.
  sID = target_GetID();
  for(ii=0;ii<MAXLUNS;ii++) {
    if(!(sID & (1<<(ii>>3)))) {
      lun[ii].Enabled = 0;
    }
  }

  // If LUNs not setup by config.tse, add them here.
  screenStatus("TABLE");
  if(!luns && !stat) {
    pTable[0].Offset = 0;
    pTable[0].Extended = 0;
    for(ii=0;ii<pTables;ii++) {
      ParseMBR(ii);
    }
  }

  DEBUGPRINT(DBG_GENERAL, "TSE %s\r\n", strSize);

  screenStatus("AUTOEXEC");
  execHandler((char*)"autoexec.tse");

  screenStatus("TARGET");
  if(target_interrupt == 1)
    target_Process();
  
  // Show the prompt
#ifdef DEBUG
  cmdDisplay();
#endif
}

void displayRefresh() {
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

elapsedMillis refreshTimer;
void loop() {
  delay(1);

//  if(refreshTimer > 300) {
//    refreshTimer = 0;
//    displayRefresh();
//  }

  // If we have an exec command pending, copy it, clear it, run it.
  if(!execLoop()) {
#ifdef DEBUG
    cmdPoll();
#endif
  }
}
