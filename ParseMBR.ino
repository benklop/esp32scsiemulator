#include "config.h"
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
    uint8_t tLUN;

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
        // Find first unused disk LUN
        for(tLUN = 0; tLUN < MAXLUNS; tLUN++) {
           if (!lun[tLUN].Enabled) continue;
           if (lun[tLUN].Type != 0) continue;
           if (lun[tLUN].Size != 0) continue;
           break;
        }
        if(tLUN < MAXLUNS) {
          lun[tLUN].Type = LUN_DISK_GENERIC;
          lun[tLUN].Offset = pTable[Table].Offset + pOffset;
          lun[tLUN].Size = pSize;
          lun[tLUN].Sectors = 16;
          lun[tLUN].SectorSize = 512;
          lun[tLUN].Heads = 16;
          lun[tLUN].Cylinders = pSize / (lun[tLUN].Heads * lun[tLUN].Sectors);
          lun[tLUN].Mounted = 1;
          luns++;
        }
        break;
#ifdef SUPPORT_OPTICAL
      case 0xf9: // Optical LUN
        for(tLUN = 0; tLUN < MAXLUNS; tLUN++) {
           if (!lun[tLUN].Enabled) continue;
           if (lun[tLUN].Type != 0) continue;
           if (lun[tLUN].Size != 0) continue;
           break;
        }
        if(tLUN < MAXLUNS) {
          lun[tLUN].Type = LUN_OPTICAL_GENERIC;
          lun[tLUN].Offset = pTable[Table].Offset + pOffset;
          lun[tLUN].Size = pSize;
          lun[tLUN].Sectors = 1;
          lun[tLUN].SectorSize = 2048;
          lun[tLUN].Heads = 1;
          lun[tLUN].Cylinders = (pSize * 512) / (lun[tLUN].Heads * lun[tLUN].Sectors * lun[tLUN].SectorSize);
          lun[tLUN].Mounted = 1;
          luns++;
        }
        break;
#endif
#ifdef SUPPORT_ETHERNET_CABLETRON
      case 0xfa: // Ethernet Cabletron
        for(tLUN = 0; tLUN < MAXLUNS; tLUN++) {
           if (!lun[tLUN].Enabled) continue;
           if (lun[tLUN].Type != 0) continue;
           if (lun[tLUN].Size != 0) continue;
           break;
        }
        if(tLUN < MAXLUNS) {
          lun[tLUN].Type = LUN_ETHERNET_CABLETRON;
          lun[tLUN].Offset = pTable[Table].Offset + pOffset;
          lun[tLUN].Size = pSize;
          luns++;
        }
        break;
#endif
#ifdef SUPPORT_ETHERNET_SCSILINK
      case 0xfb: // Ethernet SCSILink
        for(tLUN = 0; tLUN < MAXLUNS; tLUN++) {
           if (!lun[tLUN].Enabled) continue;
           if (lun[tLUN].Type != 0) continue;
           if (lun[tLUN].Size != 0) continue;
           break;
        }
        if(tLUN < MAXLUNS) {
          lun[tLUN].Type = LUN_ETHERNET_SCSILINK;
          lun[tLUN].Offset = pTable[Table].Offset + pOffset;
          lun[tLUN].Size = pSize;
          luns++;
        }
        break;
#endif
    }
    if(pType)
      pTable[Table].Part[part].Offset = pTable[Table].Offset + pOffset;
  }
}

