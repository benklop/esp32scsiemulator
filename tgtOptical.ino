#include "config.h"
#include "scsiCommands.h"
#include "scsiSense.h"
#include "LUNS.h"

static uint8_t TARGET_OPTICAL_INQUIRY_RESPONSE[96] = {
  0x05, 0x80, 0x05, 0x02,
  0x24, 0x00, 0x00, 0x00,
  // Vendor ID (8 Bytes)
  'T','i','n','y','S','C','S','I',
  // Product ID (16 Bytes)
  'T','S','E','C','D','R','O','M',' ',' ',' ',' ',' ',' ',' ',' ',
  // Revision Number (4 Bytes)
  '0','0','0','0',
  // Serial Number (8 Bytes)
  '0','0','0','0','0','0','0','0',
  // Reserved Bytes (Not really)
};

// Inquiry Command
int target_OpticalInquiry() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > Inquiry");
  if(target_cmdbuf[1] & 0x3) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 1
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x01;
    return 0;
  }

  // We only support page 0
  if(target_cmdbuf[2] != 0) {
    return 0;
  }

  // Setup sense response
  memcpy(TARGET_RESPBUF, TARGET_OPTICAL_INQUIRY_RESPONSE, sizeof(TARGET_OPTICAL_INQUIRY_RESPONSE));
  len = 5 + TARGET_RESPBUF[4];
  if(target_lun >= luns) {
    TARGET_RESPBUF[0] = 0x7f;
  }
  if(lun[target_lun].Vendor[0]) {
    memcpy(TARGET_RESPBUF+8, lun[target_lun].Vendor, 8);
  }
  if(lun[target_lun].Model[0]) {
    memcpy(TARGET_RESPBUF+16, lun[target_lun].Model, 16);
  }
  if(lun[target_lun].Revision[0]) {
    memcpy(TARGET_RESPBUF+32, lun[target_lun].Revision, 4);
  }
  if(lun[target_lun].Unique[0]) {
    memcpy(TARGET_RESPBUF+36, lun[target_lun].Unique, 8);
  } else {
    // Make serial number unique per LUN
    TARGET_RESPBUF[40] = '0' + ((target_lun >> 3) & 7);
    TARGET_RESPBUF[41] = '0' + (target_lun & 7);
  }

  // Truncate if necessary
  if(target_cmdbuf[4] < len)
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

int target_OpticalModeSense6() {
  unsigned int len;
  uint8_t page, pagemax, pagemin;
  DEBUGPRINT(DBG_TRACE, " > Mode Sense (6)");
  /* Check whether medium is present */
  if(target_CheckMedium()) return 0;

  memset(TARGET_RESPBUF, 0, sizeof(TARGET_RESPBUF));

  len = 1;
  /* Default medium type */
  TARGET_RESPBUF[len++] = 0;
  /* Write protected */
  TARGET_RESPBUF[len++] = 0x80;

  /* Add block descriptor if DBD is not set */
  if (target_cmdbuf[1] & 0x08) {
    TARGET_RESPBUF[len++] = 0;             /* No block descriptor */
  } else {
    TARGET_RESPBUF[len++] = 8;             /* Block descriptor length */
    TARGET_RESPBUF[len++] = 0;             /* Block descriptor */
    TARGET_RESPBUF[len++] = 0;             /* Volume Size */
    TARGET_RESPBUF[len++] = 0;
    TARGET_RESPBUF[len++] = 0;
    TARGET_RESPBUF[len++] = 0;             /* Sector Size */
    TARGET_RESPBUF[len++] = 0;
    TARGET_RESPBUF[len++] = 8;
    TARGET_RESPBUF[len++] = 0;
  }

  /* Check for requested mode page */
  page = target_cmdbuf[2] & 0x3F;
  pagemax = (page != 0x3f) ? page : 0x3e;
  pagemin = (page != 0x3f) ? page : 0x00;
  for(page = pagemax; page >= pagemin; page--) {
    switch (page) {
      case MODEPAGE_VENDOR_SPECIFIC:
        /* Accept request only for current values */
        if (target_cmdbuf[2] & 0xC0) {
          /* Prepare sense data */
          target_status = STATUS_CHECK;
          target_sense[target_lun].key = ILLEGAL_REQUEST;
          target_sense[target_lun].code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
          target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
          target_sense[target_lun].key_specific[1] = 0x00;
          target_sense[target_lun].key_specific[2] = 0x02;
          return 0;
        }

        /* Unit attention */
        TARGET_RESPBUF[len++] = 0x80; // PS, page id
        TARGET_RESPBUF[len++] = 0x02; // Page length
        TARGET_RESPBUF[len++] = 0x00;
        TARGET_RESPBUF[len++] = 0x00;
        break;
      case MODEPAGE_DCRC_PARAMETERS:
        TARGET_RESPBUF[len++] = 0x82; // PS, page id
        TARGET_RESPBUF[len++] = 0x0e; // Page length
        TARGET_RESPBUF[len++] = 0xe6; // Buffer full ratio, 90%
        TARGET_RESPBUF[len++] = 0x1a; // Buffer empty ratio, 10%
        TARGET_RESPBUF[len++] = 0x00; // Bus inactivity limit
        TARGET_RESPBUF[len++] = 0x00;
        TARGET_RESPBUF[len++] = 0x00; // Disconnect time limit
        TARGET_RESPBUF[len++] = 0x00;
        TARGET_RESPBUF[len++] = 0x00; // Connect time limit
        TARGET_RESPBUF[len++] = 0x00;
        TARGET_RESPBUF[len++] = 0x00; // Maximum burst size
        TARGET_RESPBUF[len++] = 0x00;
        TARGET_RESPBUF[len++] = 0x00; // EMDP, Dimm, DTDC
        TARGET_RESPBUF[len++] = 0x00; // Reserved
        TARGET_RESPBUF[len++] = 0x00; // Reserved
        TARGET_RESPBUF[len++] = 0x00; // Reserved
        break;
      default:
        if(pagemin == pagemax) {
          /* Requested mode page is not supported */
          /* Prepare sense data */
          target_status = STATUS_CHECK;
          target_sense[target_lun].key = ILLEGAL_REQUEST;
          target_sense[target_lun].code = INVALID_FIELD_IN_CDB;       /* "Invalid field in CDB" */
          target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
          target_sense[target_lun].key_specific[1] = 0x00;
          target_sense[target_lun].key_specific[2] = 0x02;
          return 0;
        }
    }
  }

  /* Report size of requested data */
  TARGET_RESPBUF[0] = len;

  /* Truncate data if necessary */
  if (target_cmdbuf[4] < len) {
    len = target_cmdbuf[4];
  }

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

// Get Unit Capacity
int target_OpticalCapacity() {
  uint32_t cap = (lun[target_lun].Cylinders * lun[target_lun].Heads * lun[target_lun].Sectors) - 1;
  DEBUGPRINT(DBG_TRACE, " > Capacity");
  if(target_CheckMedium()) return 0;

  TARGET_RESPBUF[0] = (cap >> 24) & 0xff;
  TARGET_RESPBUF[1] = (cap >> 16) & 0xff;
  TARGET_RESPBUF[2] = (cap >> 8) & 0xff;
  TARGET_RESPBUF[3] = cap & 0xff;

  // Bytes/sector
  TARGET_RESPBUF[4] = (lun[target_lun].SectorSize >> 24) & 0xff;
  TARGET_RESPBUF[5] = (lun[target_lun].SectorSize >> 16) & 0xff;
  TARGET_RESPBUF[6] = (lun[target_lun].SectorSize >> 8) & 0xff;
  TARGET_RESPBUF[7] = (lun[target_lun].SectorSize) & 0xff;

  // Send it
  return target_DIN(TARGET_RESPBUF, 8, 1);
}

// Read Updated Block
int target_CommandReadUpdatedBlock() {
//  uint32_t sectorOffset;
//  uint32_t sectorCount;

  DEBUGPRINT(DBG_GENERAL, "\n\r\t>Read Updated Block(10)");
#if 0
  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = 1;

  // Read Sectors
  return ncrReadSectors(sectorOffset, sectorCount);
#endif
  return 0;
}

int target_OpticalProcess() {
    switch(target_cmdbuf[0]) {
      case CMD_TEST_UNIT_READY:     return target_CommandTestReady();
      case CMD_REZERO_UNIT:         return target_DiskReZero();
      case CMD_READ6:               return target_DiskRead();
      case CMD_MODE_SENSE6:         return target_OpticalModeSense6();
      case CMD_START_STOP_UNIT:     return target_CommandStartStop();
      case CMD_SEND_DIAGNOSTIC:     return target_CommandSendDiag();
      case CMD_READ_CAPACITY10:     return target_OpticalCapacity();
      case CMD_READ10:              return target_DiskGroup1Read();
      case CMD_READLONG10:          return target_DiskGroup1ReadLong();
      case CMD_READUPDATEDBLOCK10:  return target_CommandReadUpdatedBlock();
    }
    return -1;
}

