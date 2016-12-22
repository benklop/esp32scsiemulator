#include "config.h"
#include "DebugConsole.h"
#include <Cmd.h>
#include "sdhc.h"
#include "ff.h"
#include "cmdDispatch.h"
#include "ParseMBR.h"
#include "LUNS.h"
#include "drvNCR5380.h"

#include "svcScreen.h"
#include "svcEthernet.h"
#include "svcTarget.h"
#include "svcInitiator.h"

int ncr_dmamode = DMA_REAL;
int phasedelay = 1;
int verbosity = DBG_INFO | DBG_WARN | DBG_ERROR | DBG_GENERAL;
extern int target_interrupt;

extern SD_CARD_DESCRIPTOR sdCardDesc;

void setup() {
  int ii;
  uint8_t sID;
  char strSize[16];

#ifdef DEBUG_WAIT
  while(!Serial);
#endif

#if defined(SUPPORT_OLED_DISPLAY)
  // Initialize OLED screen
  screenInit();
#endif

  // Initialize SCSI interface
  ncr_Init();

#if defined(SUPPORT_ETHERNET_CABLETRON) || defined(SUPPORT_ETHERNET_SCSILINK)
  // Initialize Ethernet chip
  ethernetInit();
#endif

  // Initialize SDHC Driver
  DSTATUS stat = SDHC_InitCard();

#if defined(SUPPORT_OLED_DISPLAY)
  screenCardSize(sdCardDesc.numBlocks);
#endif

  // Initialize our command executive
  cmdInit();
  dispatchInit();
  execInit();

  memset(lun, 0, sizeof(lun));
  // Enable default LUNs
  for(ii=0;ii<MAXLUNS;ii+=8) {
    lun[ii].Enabled = 1;
    lun[ii].Type = LUN_DISK_GENERIC;
  }

  execHandler((char*)"config.tse");

  // Turn off LUNs for IDs we aren't listening to.
  sID = target_GetID();
  for(ii=0;ii<MAXLUNS;ii++) {
    if(!(sID & (1<<(ii>>3)))) {
      lun[ii].Enabled = 0;
    }
  }

  // If LUNs not setup by config.tse, add them here.
  if(!luns && !stat) {
    pTable[0].Offset = 0;
    pTable[0].Extended = 0;
    for(ii=0;ii<pTables;ii++) {
      ParseMBR(ii);
    }
  }

  DEBUGPRINT(DBG_GENERAL, "TSE %s\r\n", strSize);

  execHandler((char*)"autoexec.tse");

  if(target_interrupt == 1)
    target_Process();
  
  // Show the prompt
#ifdef DEBUG
  cmdDisplay();
#endif
}

#ifdef SUPPORT_OLED_DISPLAY
elapsedMillis refreshTimer;
#endif

void loop() {
  delay(1);

#ifdef SUPPORT_OLED_DISPLAY
  if(refreshTimer > 300) {
    refreshTimer = 0;
    screenRefresh();
  }
#endif

#if defined(SUPPORT_ETHERNET_CABLETRON) || defined(SUPPORT_ETHERNET_SCSILINK)
  ethernetService();
#endif

  // If we have an exec command pending, copy it, clear it, run it.
  if(!execLoop()) {
#ifdef DEBUG
    cmdPoll();
#endif
  }
}
