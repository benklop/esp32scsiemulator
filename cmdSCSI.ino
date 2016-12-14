#include "DebugConsole.h"
#include <Cmd.h>
#include "sdhc.h"
#include <SPI.h>  // Include SPI if you're using SPI
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include "ff.h"

void scsi_init(int argc, char **argv) {
  oled.rectFill(0, 10, oled.getLCDWidth(), 8, BLACK, NORM);
  oled.setCursor(3,10);
  oled.print("Init SCSI");
  oled.display();

  ncr_Init();
}

void scsi_target(int argc, char **argv) {
  oled.rectFill(0, 10, oled.getLCDWidth(), 8, BLACK, NORM);
  oled.setCursor(12,10);
  oled.print("Target");
  oled.display();

  target_Process();
}

void scsi_print(int argc, char **argv) {
  ncr_Print();
}

void scsi_read(int argc, char **argv) {
  uint32_t reg = cmdStr2Num(argv[1], 10);
  uint8_t dat;
  dat = ncr_ReadRegister (reg & 7);
  DEBUGPRINT(DBG_GENERAL, "%d: 0x%02x\r\n", reg, dat);
}

void scsi_write(int argc, char **argv) {
  uint32_t reg = cmdStr2Num(argv[1], 10);
  uint32_t dat = cmdStr2Num(argv[2], 10);
  DEBUGPRINT(DBG_GENERAL, "%d: 0x%02x\r\n", reg, dat);
  ncr_WriteRegister(reg & 7, dat & 0xff);
}

void scsi_reset(int argc, char **argv) {
  if(argc > 1) {
    uint32_t val = cmdStr2Num(argv[1], 10);
    ncr_Reset((val&1));
  } else {
    ncr_Reset(LOW);
    delayMicroseconds(10);
    ncr_Reset(HIGH);
  }
}

void scsi_id(int argc, char **argv) {
  if(argc > 2) {
    uint32_t sID = cmdStr2Num(argv[1], 10);
    if(!strcmp(argv[2], "enable")) {
      target_SetID(sID);
    } else
    if(!strcmp(argv[2], "disable")) {
      target_ClearID(sID);
    }
  } else if(argc > 1) {
    uint32_t sID = cmdStr2Num(argv[1], 10);
    if((1<<sID) & target_GetID()) {
      DEBUGPRINT(DBG_GENERAL, "%d ON\r\n", sID);
    } else {
      DEBUGPRINT(DBG_GENERAL, "%d OFF\r\n", sID);
    }
  } else {
    DEBUGPRINT(DBG_GENERAL, "%d\r\n", target_GetID());
  }
}

Commands_t SCSICommands[] = {
  { "init", 0, "Initialize SCSI Controller", NULL, scsi_init, NULL },
  { "reset", 0, "Reset SCSI Controller", "[bool]\r\n\tResets SCSI Controller or sets state of RESET line.", scsi_reset, NULL },
  { "target", 0, "Start Target Mode", NULL, scsi_target, NULL },
  { "print", 0, "Dump SCSI Registers", NULL, scsi_print, NULL },
  { "read", 1, "Read SCSI Registers", "<addr>\r\n\tReads SCSI register <addr>", scsi_read, NULL },
  { "write", 2, "Write SCSI Registers", "<addr> <data>\r\n\tWrites base 10 <data> to SCSI register <addr>", scsi_write, NULL },
  { "id", 0, "Setup SCSI IDs", "[id] [enable|disable]\r\n\tGet/Set/Clear SCSI IDs", scsi_id, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};

void scsicmd(int argc, char **argv) {
  cmdDispatch(SCSICommands, argc, argv);
}

