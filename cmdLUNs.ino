#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>
#include <errno.h>
#include <malloc.h>

#define BLKSIZE 128

void copyblocks(uint32_t dst, uint32_t src, uint32_t len) {
  DRESULT rc=(DRESULT)0;         /* Result code */
  uint32_t blocks = 0;
  uint8_t *sdhcbuf;

  DEBUGPRINT(DBG_GENERAL, "Copy %d Blocks from %d to %d\r\n", len, src, dst);

  sdhcbuf = (uint8_t *)memalign(16, BLKSIZE*512);
  if(!sdhcbuf) {
    die("Malloc", errno);
    return;
  }

  while(blocks < len) {
    int32_t tg = (len-blocks);
    uint32_t bx = min(BLKSIZE, tg);

    DEBUGPRINT(DBG_GENERAL, "\r[%d+%d]", blocks, bx);

    rc = SDHC_ReadBlocks((UCHAR*) sdhcbuf, (DWORD) src + blocks, bx);
    if (rc) die("Read",rc);
    SDHC_DMAWait();

    //write data to card 
    rc = SDHC_WriteBlocks((UCHAR*) sdhcbuf, (DWORD) dst + blocks, bx);
    if (rc) die("Write",rc);
    SDHC_DMAWait();

    blocks += bx;
  }

  free(sdhcbuf);

  DEBUGPRINT(DBG_GENERAL, "\r\nCopied %u blocks.\r\n", len);
}

void lun_copy(int argc, char **argv) {
  if(argc > 2) {
    uint32_t pA = cmdStr2Num(argv[1], 10);
    uint32_t pB = cmdStr2Num(argv[2], 10);
    if((pA < luns) && (pB < luns) && lun[pA].Size && lun[pB].Size)
      copyblocks(lun[pB].Offset, lun[pA].Offset, min(lun[pB].Size, lun[pA].Size));
    else
      DEBUGPRINT(DBG_GENERAL, "Copy Aborted. Invalid Src / Dst pair.\r\n");
  } else {
    cmdCommandHelp(LUNCommands, 0);
  }
}

// Copy a string and space-pad it to length.
void spcopy(uint8_t *dst, char *src, int len) {
  int i;

  for(i = 0; i < len; i++) {
    if(src[i]) {
      dst[i] = src[i];
    } else {
      break;
    }
  }
  while(i < len) {
    dst[i] = ' ';
    i++;
  }
}

void lun_setup(int argc, char **argv) {
  int ii;
  uint32_t sLUN = cmdStr2Num(argv[1], 10) & 63;

  if(argc > 2) {
    for(ii = 2; ii < argc; ii++) {
      if(!strcmp(argv[ii], "enable")) {
        lun[sLUN].Enabled = 1;
        continue;
      }
      if(!strcmp(argv[ii], "disable")) {
        lun[sLUN].Enabled = 0;
        continue;
      }
      if(!strcmp(argv[ii], "disk")) {
        lun[sLUN].Type = LUN_DISK_GENERIC;
        continue;
      }
#ifdef SUPPORT_OPTICAL
      if(!strcmp(argv[ii], "optical")) {
        lun[sLUN].Type = LUN_OPTICAL_GENERIC;
        continue;
      }
#endif
#ifdef SUPPORT_ETHERNET_CABLETRON
      if(!strcmp(argv[ii], "eth_cabletron")) {
        lun[sLUN].Type = LUN_ETHERNET_CABLETRON;
        continue;
      }
#endif
#ifdef SUPPORT_ETHERNET_SCSILINK
      if(!strcmp(argv[ii], "eth_scsilink")) {
        lun[sLUN].Type = LUN_ETHERNET_SCSILINK;
        continue;
      }
#endif
      if(!strcmp(argv[ii], "offset")) {
        lun[sLUN].Offset = cmdStr2Num(argv[ii+1], 10);
        ii++;
        continue;
      }
      if(!strcmp(argv[ii], "size")) {
        lun[sLUN].Size = cmdStr2Num(argv[ii+1], 10);
        ii++;
        continue;
      }
      if(!strcmp(argv[ii], "vendor")) {
        spcopy(lun[sLUN].Vendor, argv[ii+1], 8);
        ii++;
        continue;
      }
      if(!strcmp(argv[ii], "model")) {
        spcopy(lun[sLUN].Model, argv[ii+1], 16);
        ii++;
        continue;
      }
      if(!strcmp(argv[ii], "revision")) {
        spcopy(lun[sLUN].Revision, argv[ii+1], 4);
        ii++;
        continue;
      }
      if(!strcmp(argv[ii], "serial")) {
        spcopy(lun[sLUN].Unique, argv[ii+1], 8);
        ii++;
        continue;
      }
    }
  } else {
    // Print LUN
    DEBUGPRINT(DBG_GENERAL, "LUN %d\r\n", sLUN);
    DEBUGPRINT(DBG_GENERAL, "Enabled: %d\r\n", lun[sLUN].Enabled);
    DEBUGPRINT(DBG_GENERAL, "Type: %d\r\n", lun[sLUN].Type);
    DEBUGPRINT(DBG_GENERAL, "Offset: %d\r\n", lun[sLUN].Offset);
    DEBUGPRINT(DBG_GENERAL, "Size: %d\r\n", lun[sLUN].Size);
  }
}

void lun_disable(int argc, char **argv) {
  for(int ii=0;ii<MAXLUNS;ii++) {
    lun[ii].Enabled = 0;
    lun[ii].Type = 0;
  }
}

Commands_t LUNCommands[] = {
  { "disable", 0, "Disable All LUNs", NULL, lun_disable, NULL },
  { "copy", 2, "Copy Partition", "<a> <b>\r\n\tCopy Partition <a> to <b>", lun_copy, NULL },
  { "set", 2, "Setup LUNs", "<lun> [enable|disable] [type] [offset <n>] [size <n>]", lun_setup, NULL },
  { NULL, 0, NULL, NULL }
};

void luncmd(int argc, char **argv) {
  cmdDispatch(LUNCommands, argc, argv);
}

