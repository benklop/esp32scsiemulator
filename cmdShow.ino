#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>

typedef struct PartType_s {
  unsigned char Type;
  const char *Name;
} PartType_t;
PartType_t PartType[] = {
  { 0x00, "Empty" },
  { 0x01, "FAT12" },
  { 0x04, "FAT16 <32MB" },
  { 0x05, "Extended" },
  { 0x06, "FAT16 >32MB" },
  { 0x07, "NTFS" },
  { 0x0b, "FAT32" },
  { 0x0c, "FAT32 LBA" },
  { 0x0e, "FAT16 LBA" },
  { 0x0f, "Extended LBA" },
  { 0xf8, "TinySCSI Disk LUN" },
  { 0xf9, "TinySCSI Optical LUN" },
  { 0xfa, "Cabletron Ethernet LUN" },
  { 0xfb, "SCSI/Link Ethernet LUN" },
  { 0xff, NULL }
};


//#define COUNT 4// multiblock count
//#define MBLK 6
//uint8_t buffer[(1<<MBLK)*512] __attribute__((aligned(4)));
extern uint8_t m_sdhc_CMD6_Status[];
extern uint32_t m_sdhc_ocr;
extern SD_CARD_DESCRIPTOR sdCardDesc;

void show_sdhc(int argc, char **argv) {
  DEBUGPRINT(DBG_GENERAL, "%d Hz", SDHC_Baudrate());
  DEBUGPRINT(DBG_GENERAL, "\tDMA: %d", SDHC_TRANSFERTYPE == SDHC_TRANSFERTYPE_DMA);
  DEBUGPRINT(DBG_GENERAL, "\tocr: %x", m_sdhc_ocr);
  DEBUGPRINT(DBG_GENERAL, "\r\nStatus: %d", sdCardDesc.status);
  DEBUGPRINT(DBG_GENERAL, "\tAddress: %x", sdCardDesc.address);
  DEBUGPRINT(DBG_GENERAL, "\r\nHC: %d", sdCardDesc.highCapacity);
  DEBUGPRINT(DBG_GENERAL, "\tV2: %d", sdCardDesc.version2);
  DEBUGPRINT(DBG_GENERAL, "\r\nBlocks: %u (%u MB)", sdCardDesc.numBlocks, sdCardDesc.numBlocks/2048);
  DEBUGPRINT(DBG_GENERAL, "\tLast Status: %d\r\n\r\n", sdCardDesc.lastCardStatus);
}

void show_part(int argc, char **argv) {
  int Table, Part;
  int verbose = 0;

  if(argc > 1) {
    if(!strcmp(argv[1], "verbose")) {
      verbose = 1;
    }
  }  
  DEBUGPRINT(DBG_GENERAL, " Aggregate Partition Table\r\n");
  DEBUGPRINT(DBG_GENERAL, "     Start \t      End \t    Size \tType\r\n");

  for(Table=0; Table<pTables; Table++) {

    if(verbose)
      DEBUGPRINT(DBG_GENERAL, "Table %d at %d\r\n", Table, pTable[Table].Offset);

    for(Part=0; Part<4; Part++) {
      if(!verbose) {
        if(pTable[Table].Part[Part].Type == 0x00) continue; // Hide Empty entries from the report
        if(pTable[Table].Part[Part].Type == 0x05) continue; // Hide Extended entries from the report
        if(pTable[Table].Part[Part].Type == 0x0f) continue; // Hide Extended LBA entries from the report
      }
      DEBUGPRINT(DBG_GENERAL, " %9d \t%9d \t%08x \t%02x ",
                                       pTable[Table].Part[Part].Offset,
                                       (pTable[Table].Part[Part].Offset + pTable[Table].Part[Part].Size),
                                       pTable[Table].Part[Part].Size,
                                       pTable[Table].Part[Part].Type); 

      for(int pt=0; PartType[pt].Name!=NULL; pt++) {
        if(pTable[Table].Part[Part].Type == PartType[pt].Type) {
          DEBUGPRINT(DBG_GENERAL, "%s", PartType[pt].Name);
          break;
        }
      }
      DEBUGPRINT(DBG_GENERAL, "\r\n");
    }
  }
}

void show_luns(int argc, char **argv) {
  int ii;
  DEBUGPRINT(DBG_GENERAL, "       Start \t      End \t    Size\r\n");
  for(ii=0; ii<MAXLUNS; ii++) {
    if(lun[ii].Enabled) {
      switch(lun[ii].Type) {
        case LUN_DISK_GENERIC:
          DEBUGPRINT(DBG_GENERAL, "%d: %9d \t%9d \t%08x DISK\r\n", ii, lun[ii].Offset, (lun[ii].Offset + lun[ii].Size), lun[ii].Size); 
          break;
        case LUN_OPTICAL_GENERIC:
          DEBUGPRINT(DBG_GENERAL, "%d: %9d \t%9d \t%08x CDROM\r\n", ii, lun[ii].Offset, (lun[ii].Offset + lun[ii].Size), lun[ii].Size); 
          break;
        case LUN_ETHERNET_CABLETRON:
          DEBUGPRINT(DBG_GENERAL, "%d: %9d \t%9d \t%08x ETH CABLETRON\r\n", ii, lun[ii].Offset, (lun[ii].Offset + lun[ii].Size), lun[ii].Size); 
          break;
        case LUN_ETHERNET_SCSILINK:
          DEBUGPRINT(DBG_GENERAL, "%d: %9d \t%9d \t%08x ETH SCSILINK\r\n", ii, lun[ii].Offset, (lun[ii].Offset + lun[ii].Size), lun[ii].Size); 
          break;
        case LUN_TAPE_GENERIC:
          DEBUGPRINT(DBG_GENERAL, "%d: %9d \t%9d \t%08x TAPE\r\n", ii, lun[ii].Offset, (lun[ii].Offset + lun[ii].Size), lun[ii].Size); 
          break;
      }
    }
  }
}

Commands_t ShowCommands[] = {
  { "luns", 0, "Virtual LUNs", NULL, show_luns, NULL },
  { "part", 0, "Partition Tables", "[verbose]\r\n\tPartition Tables", show_part, NULL },
  { "sdhc", 0, "SDHC Card Info", NULL, show_sdhc, NULL },
  { NULL, 0, NULL, NULL }
};

void showcmd(int argc, char **argv) {
  cmdDispatch(ShowCommands, argc, argv);
}
