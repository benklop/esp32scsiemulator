#include "config.h"
#include "scsiCommands.h"
#include "scsiSense.h"
#include "LUNS.h"

static uint8_t TARGET_DISKBUF[64*1024];
static uint8_t TARGET_VERIFYBUF[512];

static uint8_t TARGET_INQUIRY_RESPONSE[] = {
  0x00, 0x00, 0x01, 0x01,
  0x32, 0x00, 0x00, 0x00,
  // Vendor ID (8 Bytes)
  'T','i','n','y','S','C','S','I',
  // Product ID (16 Bytes)
  'T','S','E','D','i','s','k',' ',
  ' ',' ',' ',' ',' ',' ',' ',' ',
  // Revision Number (4 Bytes)
  '0','0','0','0',
  // Serial Number (8 Bytes)
  '0','0','0','0','0','0','0','0',
  // Reserved Bytes (Not really)
};

// Inquiry Command
int target_DiskInquiry() {
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
  
  // Setup sense response
  memcpy(TARGET_RESPBUF, TARGET_INQUIRY_RESPONSE, sizeof(TARGET_INQUIRY_RESPONSE));
  len = 5 + TARGET_RESPBUF[4];
  if(!lun[target_lun].Enabled) {
    TARGET_RESPBUF[0] = 0x7f;
  }

  if(lun[target_lun].Vendor[0]) {
    memcpy(TARGET_RESPBUF+8, lun[target_lun].Vendor, 8);
  }
  if(lun[target_lun].Model[0]) {
    memcpy(TARGET_RESPBUF+16, lun[target_lun].Model, 16);
  } else {
    if(lun[target_lun].Size > 1024*1024*4) {
      snprintf((char *)(TARGET_RESPBUF+24), 8, "%5dGB", (int)(lun[target_lun].Size / (2 * 1024 * 1024)));
    } else {
      snprintf((char *)(TARGET_RESPBUF+24), 8, "%5dMB", (int)(lun[target_lun].Size / (2 * 1024)));
    }
    TARGET_RESPBUF[31] = ' ';
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

int target_DiskModeSense6() {
  unsigned int len;
  int page, pagemax, pagemin;
  DEBUGPRINT(DBG_TRACE, " > Mode Sense (6)");
  /* Check whether medium is present */
  if(target_CheckMedium()) return 0;

  memset(TARGET_RESPBUF, 0, sizeof(TARGET_RESPBUF));

  len = 1;
  /* Default medium type */
  TARGET_RESPBUF[len++] = 0;
  /* Write protected */
  TARGET_RESPBUF[len++] = lun[target_lun].WriteProtect ? 0x80 : 0x00;

  /* Add block descriptor if DBD is not set */
  if (target_cmdbuf[1] & 0x08) {
    TARGET_RESPBUF[len++] = 0;             /* No block descriptor */
  } else {
    uint32_t cap = (lun[target_lun].Cylinders * lun[target_lun].Heads * lun[target_lun].Sectors) - 1;
    TARGET_RESPBUF[len++] = 8;             /* Block descriptor length */
    TARGET_RESPBUF[len++] = (cap >> 24) & 0xff;
    TARGET_RESPBUF[len++] = (cap >> 16) & 0xff;
    TARGET_RESPBUF[len++] = (cap >> 8) & 0xff;
    TARGET_RESPBUF[len++] = cap & 0xff;
    TARGET_RESPBUF[len++] = (lun[target_lun].SectorSize >> 24) & 0xff;
    TARGET_RESPBUF[len++] = (lun[target_lun].SectorSize >> 16) & 0xff;
    TARGET_RESPBUF[len++] = (lun[target_lun].SectorSize >> 8) & 0xff;
    TARGET_RESPBUF[len++] = (lun[target_lun].SectorSize) & 0xff;
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
      case MODEPAGE_FORMAT_PARAMETERS:
        TARGET_RESPBUF[len++] = 0x83; // PS, page id
        TARGET_RESPBUF[len++] = 0x16; // Page length
        TARGET_RESPBUF[len++] = (lun[target_lun].Cylinders * lun[target_lun].Heads) >> 8; // Track/zone
        TARGET_RESPBUF[len++] = lun[target_lun].Cylinders * lun[target_lun].Heads;        // Track/zone
        TARGET_RESPBUF[len++] = 0x00; // Alt sect/zone
        TARGET_RESPBUF[len++] = 0x00; // Alt sect/zone
        TARGET_RESPBUF[len++] = 0x00; // Alt track/zone
        TARGET_RESPBUF[len++] = 0x00; // Alt track/zone
        TARGET_RESPBUF[len++] = 0x00; // Alt track/volume
        TARGET_RESPBUF[len++] = 0x00; // Alt track/volume
        TARGET_RESPBUF[len++] = lun[target_lun].Sectors >> 8; // Sectors/track
        TARGET_RESPBUF[len++] = lun[target_lun].Sectors;      // Sectors/track
        TARGET_RESPBUF[len++] = lun[target_lun].SectorSize >> 8; // Bytes/sector
        TARGET_RESPBUF[len++] = lun[target_lun].SectorSize;      // Bytes/sector
        TARGET_RESPBUF[len++] = 0x00; // Interleave
        TARGET_RESPBUF[len++] = 0x00; // Interleave
        TARGET_RESPBUF[len++] = 0x00; // Track skew
        TARGET_RESPBUF[len++] = 0x00; // Track skew
        TARGET_RESPBUF[len++] = 0x00; // Cylinder skew
        TARGET_RESPBUF[len++] = 0x00; // Cylinder skew
        TARGET_RESPBUF[len++] = 0x00; // Sectoring type
        TARGET_RESPBUF[len++] = 0x00; // Reserved
        TARGET_RESPBUF[len++] = 0x00; // Reserved
        TARGET_RESPBUF[len++] = 0x00; // Reserved
        break;
      case MODEPAGE_RIGID_GEOMETRY:
        TARGET_RESPBUF[len++] = 0x84;                            // PS, page id
        TARGET_RESPBUF[len++] = 0x16;                            // Page length
        TARGET_RESPBUF[len++] = lun[target_lun].Cylinders >> 16; // Cylinders
        TARGET_RESPBUF[len++] = lun[target_lun].Cylinders >> 8;  // Cylinders
        TARGET_RESPBUF[len++] = lun[target_lun].Cylinders;       // Cylinders
        TARGET_RESPBUF[len++] = lun[target_lun].Heads;           // Heads
        TARGET_RESPBUF[len++] = 0x00;                            // Starting cylinder - write precomp
        TARGET_RESPBUF[len++] = 0x00;                            // Starting cylinder - write precomp
        TARGET_RESPBUF[len++] = 0x00;                            // Starting cylinder - write precomp
        TARGET_RESPBUF[len++] = 0x00;                            // Starting cylinder - reduced write current
        TARGET_RESPBUF[len++] = 0x00;                            // Starting cylinder - reduced write current
        TARGET_RESPBUF[len++] = 0x00;                            // Starting cylinder - reduced write current
        TARGET_RESPBUF[len++] = 0x00;                            // Drive step rate
        TARGET_RESPBUF[len++] = 0x00;                            // Drive step rate
        TARGET_RESPBUF[len++] = 0x00;                            // Landing zone cylinder
        TARGET_RESPBUF[len++] = 0x00;                            // Landing zone cylinder
        TARGET_RESPBUF[len++] = 0x00;                            // Landing zone cylinder
        TARGET_RESPBUF[len++] = 0x00;                            // RPL
        TARGET_RESPBUF[len++] = 0x00;                            // Rotational offset
        TARGET_RESPBUF[len++] = 0x00;                            // Reserved
        TARGET_RESPBUF[len++] = 0x27;                            // Medium rotation rate / 256
        TARGET_RESPBUF[len++] = 0x10;                            // Medium rotation rate % 256
        TARGET_RESPBUF[len++] = 0x00;                            // Reserved
        TARGET_RESPBUF[len++] = 0x00;                            // Reserved
        break;
      case MODEPAGE_APPLE:
        if(lun[target_lun].Quirks & QUIRKS_APPLE) {
          TARGET_RESPBUF[len++] = 0xb0;
          TARGET_RESPBUF[len++] = 0x16;
          TARGET_RESPBUF[len++] = 'A';
          TARGET_RESPBUF[len++] = 'P';
          TARGET_RESPBUF[len++] = 'P';
          TARGET_RESPBUF[len++] = 'L';
          TARGET_RESPBUF[len++] = 'E';
          TARGET_RESPBUF[len++] = ' ';
          TARGET_RESPBUF[len++] = 'C';
          TARGET_RESPBUF[len++] = 'O';
          TARGET_RESPBUF[len++] = 'M';
          TARGET_RESPBUF[len++] = 'P';
          TARGET_RESPBUF[len++] = 'U';
          TARGET_RESPBUF[len++] = 'T';
          TARGET_RESPBUF[len++] = 'E';
          TARGET_RESPBUF[len++] = 'R';
          TARGET_RESPBUF[len++] = ',';
          TARGET_RESPBUF[len++] = ' ';
          TARGET_RESPBUF[len++] = 'I';
          TARGET_RESPBUF[len++] = 'N';
          TARGET_RESPBUF[len++] = 'C';
          TARGET_RESPBUF[len++] = ' ';
          TARGET_RESPBUF[len++] = ' ';
          TARGET_RESPBUF[len++] = ' ';
        }
        break;
      case MODEPAGE_RW_ERROR_RECOVERY:
        /* Check for changeable values request */
        if ((target_cmdbuf[2] & 0xC0) == 0x40) {
          TARGET_RESPBUF[len++] = 0x01;          /* Page code */
          TARGET_RESPBUF[len++] = 0x0A;      /* Page size */
          /* No changeable values */
          len += 10;
        } else {
          /* Add read/write error recovery mode page */
          TARGET_RESPBUF[len++] = 0x01;          /* Page code */
          TARGET_RESPBUF[len++] = 0x0A;      /* Page size */
          TARGET_RESPBUF[len++] = lun[target_lun].ReportAutoCorrect ? 0xC0 : 0x00;   /* Config bits */
          TARGET_RESPBUF[len++] = 0;         /* Read retry count */
          TARGET_RESPBUF[len++] = 0;
          TARGET_RESPBUF[len++] = 0;
          TARGET_RESPBUF[len++] = 0;
          TARGET_RESPBUF[len++] = 0;
          TARGET_RESPBUF[len++] = 0;         /* Write retry count */
          TARGET_RESPBUF[len++] = 0;
          TARGET_RESPBUF[len++] = 0;     /* Recovery time limit (1ms) */
          TARGET_RESPBUF[len++] = 1;
        }

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
int target_DiskCapacity() {
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

// ReZero Unit - Returns with Good Status
int target_DiskReZero() {
  DEBUGPRINT(DBG_TRACE, " > ReZero");
  return 0;
}

//  Verify Command
int target_DiskGroup1Verify() {
  uint32_t sectorOffset;
  uint32_t sectorCount;
//  int tSCSI = 0;
//  int tCard = 0;
//  int tCompare = 0;
//  int tMessage = 0;

  if(target_CheckMedium()) return 0;

  lun[target_lun].Active = 0x81;

  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  DEBUGPRINT(DBG_TRACE, "\n\r> Verify Sectors %d %d", sectorOffset, sectorCount);

  // Check if sector is outside LUN
  if((sectorOffset + sectorCount) > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  // No data to check against.
  if((target_cmdbuf[1] & 0x02) == 0) {
    return 0;
  }

  sectorOffset += lun[target_lun].Offset;
  while(sectorCount > 0) {
// elapsedMillis diskTime;

//    diskTime = 0;
    // Ask host to send it
    if(target_DOUT(TARGET_VERIFYBUF, sizeof(TARGET_VERIFYBUF), 1)) return 1;
//    tSCSI += diskTime;

    // Read Sector
//    diskTime = 0;
    DRESULT res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();
//    tCard += diskTime; diskTime = 0;
    if(memcmp(TARGET_VERIFYBUF, TARGET_DISKBUF, sizeof(TARGET_VERIFYBUF))) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
//    tCompare += diskTime; diskTime = 0;
    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;
//    tMessage += diskTime;
    
    sectorCount --;
  }

//  DEBUGPRINT(DBG_TRACE, " S:%d/C:%d/P:%d/M:%d ms ", tSCSI, tCard, tCompare, tMessage);

  return 0;
}

int target_ReadBytes(uint32_t sectorOffset, uint32_t sectorCount) {
  uint32_t multiblock = (sizeof(TARGET_DISKBUF)/512);
  DEBUGPRINT(DBG_TRACE, "\n\r> R %d %d", sectorOffset, sectorCount);

  uint64_t byteOffset = ((uint64_t)sectorOffset) * lun[target_lun].SectorSize;
  sectorOffset = byteOffset / 512;
  uint64_t byteCount = ((uint64_t)sectorCount) * lun[target_lun].SectorSize;
  sectorCount = byteCount / 512;
  uint32_t sectorAlignmentOffset = byteOffset & 0x1ff;

  // If virtual sectors are smaller then we need to read an extra physical sector
  if(lun[target_lun].SectorSize < 512)
    sectorCount++;

  lun[target_lun].Active = 0x21;

  DEBUGPRINT(DBG_TRACE, " > TR %d %d", sectorOffset, sectorCount);

  // Check if sector is outside LUN
  if((sectorOffset + sectorCount) > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  sectorOffset += lun[target_lun].Offset;

  // Handle unaligned reads
  if(sectorAlignmentOffset) {
    DRESULT res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Send it
    if(target_DIN(TARGET_DISKBUF+sectorAlignmentOffset, 512-sectorAlignmentOffset, 1)) return 1;

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;

    sectorOffset ++;
    sectorCount --;
    byteOffset += 512-sectorAlignmentOffset;
    byteCount -= 512-sectorAlignmentOffset;
  }

  while(byteCount > 512) {
    if(byteCount < 512*multiblock) {
      multiblock = byteCount / 512;
    }

    DRESULT res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, multiblock);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Send it
    if(target_DIN(TARGET_DISKBUF, 512, multiblock)) return 1;

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;

    sectorOffset += multiblock;
    sectorCount -= multiblock;
    byteOffset += 512*multiblock;
    byteCount -= 512*multiblock;
  }

  if(byteCount > 0) {
    DRESULT res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Send it
    if(target_DIN(TARGET_DISKBUF, lun[target_lun].SectorSize, byteCount / lun[target_lun].SectorSize)) return 1;

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;
  }

  return 0;
}

// Read sectors from device
// TODO: Interleave Card and SCSI DMA, Make SCSI DMA servicing an ISR.
int target_ReadSectors(uint32_t sectorOffset, uint32_t sectorCount) {
  uint32_t multiblock = (sizeof(TARGET_DISKBUF)/512);

  if(lun[target_lun].SectorSize != 512) {
    return target_ReadBytes(sectorOffset, sectorCount);
  }

  lun[target_lun].Active = 0x21;

  DEBUGPRINT(DBG_TRACE, "\n\r> R %d %d", sectorOffset, sectorCount);
  // Check if sector is outside LUN
  if((sectorOffset + sectorCount) > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  sectorOffset += lun[target_lun].Offset;
  while(sectorCount > 0) {
    if(sectorCount < multiblock) {
      multiblock = sectorCount;
    }
    DRESULT res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, multiblock);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Send it
    if(target_DIN(TARGET_DISKBUF, 512, multiblock)) return 1;

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;

    sectorOffset += multiblock;
    sectorCount -= multiblock;
  }

  return 0;
}

// Write sectors to device
int target_WriteBytes(uint32_t sectorOffset, uint32_t sectorCount) {
  uint16_t multiblock = (sizeof(TARGET_DISKBUF)/512);
  DRESULT res;

  if(lun[target_lun].SectorSize != 512) {
    return target_WriteBytes(sectorOffset, sectorCount);
  }

  lun[target_lun].Active = 0x41;

  if(lun[target_lun].WriteProtect) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = MEDIUM_ERROR;
    target_sense[target_lun].code = 0x0c; // Write Error
    return 0;
  }

  uint64_t byteOffset = ((uint64_t)sectorOffset) * lun[target_lun].SectorSize;
  sectorOffset = byteOffset / 512;
  uint64_t byteCount = ((uint64_t)sectorCount) * lun[target_lun].SectorSize;
  sectorCount = byteCount / 512;
  uint32_t sectorAlignmentOffset = byteOffset & 0x1ff;

  // If virtual sectors are smaller then we need to read an extra physical sector
  if(lun[target_lun].SectorSize < 512)
    sectorCount++;

  DEBUGPRINT(DBG_TRACE, " > TR %d %d", sectorOffset, sectorCount);

  // Check if sector is outside LUN
  if((sectorOffset + sectorCount) > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  sectorOffset += lun[target_lun].Offset;

  // Handle unaligned writes
  if(sectorAlignmentOffset) {
    res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Get it
    if(target_DOUT(TARGET_DISKBUF+sectorAlignmentOffset, 512-sectorAlignmentOffset, 1)) return 1;

    res = SDHC_WriteBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;

    sectorOffset ++;
    sectorCount --;
    byteOffset += 512-sectorAlignmentOffset;
    byteCount -= 512-sectorAlignmentOffset;
  }

  while(byteCount > 512) {
    if(byteCount < 512*multiblock) {
      multiblock = byteCount / 512;
    }

    // Send it
    if(target_DOUT(TARGET_DISKBUF, 512, multiblock)) return 1;

    // Write Sector
    res = SDHC_WriteBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, multiblock);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;

    sectorOffset += multiblock;
    sectorCount -= multiblock;
    byteOffset += 512*multiblock;
    byteCount -= 512*multiblock;
  }

  if(byteCount > 0) {
    res = SDHC_ReadBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Get it
    if(target_DOUT(TARGET_DISKBUF, lun[target_lun].SectorSize, byteCount / lun[target_lun].SectorSize)) return 1;

    res = SDHC_WriteBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, 1);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;
  }

  return 0;
}

// Write sectors to device
int target_WriteSectors(uint32_t sectorOffset, uint32_t sectorCount) {
  uint16_t multiblock = (sizeof(TARGET_DISKBUF)/512);

  if(lun[target_lun].SectorSize != 512) {
    return target_WriteBytes(sectorOffset, sectorCount);
  }

  lun[target_lun].Active = 0x41;

  if(lun[target_lun].WriteProtect) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = MEDIUM_ERROR;
    target_sense[target_lun].code = 0x0c; // Write Error
    return 0;
  }

  DEBUGPRINT(DBG_TRACE, "\n\r>W %d %d", sectorOffset, sectorCount);
  // Check if sector is outside LUN
  if((sectorOffset + sectorCount) > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  sectorOffset += lun[target_lun].Offset;
  while(sectorCount > 0) {
    if(sectorCount < multiblock) {
      multiblock = sectorCount;
    }
    // Send it
    if(target_DOUT(TARGET_DISKBUF, 512, multiblock)) return 1;

    // Write Sector
    DRESULT res = SDHC_WriteBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, multiblock);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;

    sectorOffset += multiblock;
    sectorCount -= multiblock;
  }

  return 0;
}


// Group 0 Read Data
int target_DiskRead() {
  uint32_t sectorOffset;
  uint32_t sectorCount;

  DEBUGPRINT(DBG_TRACE, " > Read(6)");
  if(target_CheckMedium()) return 0;

  sectorOffset = ((target_cmdbuf[1] & 0x1F) << 16) | (target_cmdbuf[2] << 8) | target_cmdbuf[3];
  sectorCount = target_cmdbuf[4];
  if(sectorCount == 0) sectorCount = 256;

  // Read Sectors
  return target_ReadSectors(sectorOffset, sectorCount);
}

// Group 0 Write Data
int target_DiskWrite() {
  uint32_t sectorOffset;
  uint32_t sectorCount;
  
  DEBUGPRINT(DBG_TRACE, " > Write(6)");
  if(target_CheckMedium()) return 0;

  sectorOffset = ((target_cmdbuf[1] & 0x1F) << 16) | (target_cmdbuf[2] << 8) | target_cmdbuf[3];
  sectorCount = target_cmdbuf[4];
  if(sectorCount == 0) sectorCount = 256;

  // Write Sectors
  return target_WriteSectors(sectorOffset, sectorCount);
}

// Group 1 Read Data
int target_DiskGroup1Read() {
  uint32_t sectorOffset;
  uint32_t sectorCount;

  DEBUGPRINT(DBG_TRACE, " > Read(10)");
  if(target_CheckMedium()) return 0;

  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  // Read Sectors
  return target_ReadSectors(sectorOffset, sectorCount);
}

// Group 1 Write Data
int target_DiskGroup1Write() {
  uint32_t sectorOffset;
  uint32_t sectorCount;

  DEBUGPRINT(DBG_TRACE, " > Write(10)");
  if(target_CheckMedium()) return 0;

  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  // Write Sectors
  return target_WriteSectors(sectorOffset, sectorCount);
}

// Group 1 SyncCache
int target_DiskGroup1SyncCache() {
  uint32_t sectorOffset;
  uint32_t sectorCount;

  DEBUGPRINT(DBG_TRACE, " > SyncCache(10)");
  if(target_CheckMedium()) return 0;

  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  // Check if sector is outside LUN
  if((sectorOffset + sectorCount) > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  // Flush Sectors
  return 0;
}

// Group 1 Read Long Data
int target_DiskGroup1ReadLong() {
  uint32_t sectorOffset;

  DEBUGPRINT(DBG_TRACE, " > Read Long(10)");
  if(target_CheckMedium()) return 0;

  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];

  if(sectorOffset > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  if(((target_cmdbuf[7] << 8) | target_cmdbuf[8]) == 0) {
    return 0;
  } else if(((target_cmdbuf[7] << 8) | target_cmdbuf[8]) != 512) {
    int32_t sizediff = ((target_cmdbuf[7] << 8) | target_cmdbuf[8]) - 512;
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = 0x20 | ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // LBA Out of range
    target_sense[target_lun].info[0] = (sizediff >> 24) & 0xff;
    target_sense[target_lun].info[1] = (sizediff >> 16) & 0xff;
    target_sense[target_lun].info[2] = (sizediff >> 8) & 0xff;
    target_sense[target_lun].info[3] = sizediff & 0xff;
    target_sense[target_lun].key_specific[0] = 0x00;
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x00;
  }

  // Read Sectors
  return target_ReadSectors(sectorOffset, 1);
}

// Group 1 Read Long Data
int target_DiskGroup1WriteLong() {
  uint32_t sectorOffset;

  DEBUGPRINT(DBG_TRACE, " > Write Long(10)");
  if(target_CheckMedium()) return 0;

  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];

  if(sectorOffset > lun[target_lun].Size) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_LBA; // LBA Out of range
    return 0;
  }

  if(((target_cmdbuf[7] << 8) | target_cmdbuf[8]) == 0) {
    return 0;
  } else if(((target_cmdbuf[7] << 8) | target_cmdbuf[8]) != 512) {
    int32_t sizediff = ((target_cmdbuf[7] << 8) | target_cmdbuf[8]) - 512;
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = 0x20 | ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // LBA Out of range
    target_sense[target_lun].info[0] = (sizediff >> 24) & 0xff;
    target_sense[target_lun].info[1] = (sizediff >> 16) & 0xff;
    target_sense[target_lun].info[2] = (sizediff >> 8) & 0xff;
    target_sense[target_lun].info[3] = sizediff & 0xff;
    target_sense[target_lun].key_specific[0] = 0x00;
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x00;
  }

  // Read Sectors
  return target_WriteSectors(sectorOffset, 1);
}

// Format Unit - Return Good Status
int target_DiskFormat() {
  DEBUGPRINT(DBG_TRACE, " > Format");
  if(target_CheckMedium()) return 0;

  /* Check for parameter list ('FMTDATA' Bit) */
  if(target_cmdbuf[1] & 0x10) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 1
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x01;
    return 0;
  }
  return 0;
}

int target_DiskProcess() {
    switch(target_cmdbuf[0]) {
      case CMD_TEST_UNIT_READY:     return target_CommandTestReady();
      case CMD_REZERO_UNIT:         return target_DiskReZero();
      case CMD_FORMAT_UNIT:         return target_DiskFormat();
      case CMD_READ6:               return target_DiskRead();
      case CMD_WRITE6:              return target_DiskWrite();
      case CMD_MODE_SENSE6:         return target_DiskModeSense6();
      case CMD_START_STOP_UNIT:     return target_CommandStartStop();
      case CMD_SEND_DIAGNOSTIC:     return target_CommandSendDiag();
      case CMD_READ_CAPACITY10:     return target_DiskCapacity();
      case CMD_READ10:              return target_DiskGroup1Read();
      case CMD_WRITE10:             return target_DiskGroup1Write();
      case CMD_WRITEANDVERIFY10:    return target_DiskGroup1Write();
      case CMD_READLONG10:          return target_DiskGroup1ReadLong();
      case CMD_WRITELONG10:         return target_DiskGroup1WriteLong();
      case CMD_SYNCHRONIZE_CACHE10: return target_DiskGroup1SyncCache();
      case CMD_VERIFY10:            return target_DiskGroup1Verify();
    }
    return -1;
}

