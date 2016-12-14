#include "DebugConsole.h"
#include "LUNS.h"
#include "sdhc.h"

TABLE_t pTable[MAXTABLES];
volatile int pTables = 1;

void ParseMBR(int Table) {
  uint8_t mbrbuf[512];
  int part;

  for(int ii=0;ii<512;ii++) mbrbuf[ii]=0xff;

  // Read table
  /* DRESULT res = */ SDHC_ReadBlocks((UCHAR*) mbrbuf, (DWORD) pTable[Table].Offset, 1);
  SDHC_DMAWait();

  for(part = 0; part < 4; part++) {
    uint8_t *entry = &mbrbuf[446+16*part];
    uint32_t pOffset = *(uint32_t*)&entry[8];
    uint32_t pSize = *(uint32_t*)&entry[12];
    uint8_t pType = entry[4];

    pTable[Table].Part[part].Size = pSize;
    pTable[Table].Part[part].Type = pType;

    switch(pType) {
      case 0x00: // Empty
        pTable[Table].Part[part].Offset = pOffset;
        break;
      case 0x05: // Extended
        if(pTables < MAXTABLES) {
          pTable[Table].Part[part].Offset = pTable[Table].Extended + pOffset;
          pTable[pTables].Extended = pTable[Table].Extended;
          pTable[pTables].Offset = pTable[Table].Extended + pOffset;
          pTables++;
        }
        break;
      case 0x0f: // Extended LBA
        if(pTables < MAXTABLES) {
          pTable[Table].Part[part].Offset = pTable[Table].Extended + pOffset;
          pTable[pTables].Extended = pTable[Table].Extended + pOffset;
          pTable[pTables].Offset = pTable[Table].Extended + pOffset;
          pTables++;
        }
        break;
      case 0xf8: // Disk LUN
        while((luns < MAXLUNS) && (!lun[luns].Enabled) && (lun[luns].Type |= LUN_DISK) ) luns++;
        if(luns < MAXLUNS) {
          lun[luns].Type = LUN_DISK;
          lun[luns].Offset = pTable[Table].Offset + pOffset;
          lun[luns].Size = pSize;
          lun[luns].Mounted = 1;
          luns++;
        }
#ifdef SUPPORT_OPTICAL
      case 0xf9: // Optical LUN
        while((luns < MAXLUNS) && (!lun[luns].Enabled) && (lun[luns].Type |= LUN_OPTICAL) ) luns++;
        if(luns < MAXLUNS) {
          lun[luns].Type = LUN_OPTICAL;
          lun[luns].Offset = pTable[Table].Offset + pOffset;
          lun[luns].Size = pSize;
          lun[luns].Mounted = 1;
          luns++;
        }
#endif
      default:
        pTable[Table].Part[part].Offset = pTable[Table].Offset + pOffset;
    }
  }
}

