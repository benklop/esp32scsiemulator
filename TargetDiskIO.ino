#include "SCSI_Commands.h"
#include "SCSI_Sense.h"
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

//  Verify Command
int target_CommandGroup1Verify() {
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

// Read sectors from device
// TODO: Interleave Card and SCSI DMA, Make SCSI DMA servicing an ISR.
int target_ReadSectors(uint32_t sectorOffset, uint32_t sectorCount) {
  uint16_t multiblock = (sizeof(TARGET_DISKBUF)/512);
  int tSCSI = 0;
  int tCard = 0;
  int tMessage = 0;

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
    elapsedMillis diskTime;
    diskTime = 0;
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
    tCard += diskTime; diskTime = 0;

    // Send it
    if(target_DIN(TARGET_DISKBUF, 512, multiblock)) return 1;
    tSCSI += diskTime; diskTime = 0;

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;
    tMessage += diskTime; diskTime = 0;

    sectorOffset += multiblock;
    sectorCount -= multiblock;
  }

  DEBUGPRINT(DBG_TRACE, " S:%d/C:%d/M:%d ms ", tSCSI, tCard, tMessage);

  return 0;
}

// Write sectors to device
int target_WriteSectors(uint32_t sectorOffset, uint32_t sectorCount) {
  uint16_t multiblock = (sizeof(TARGET_DISKBUF)/512);
  int tSCSI = 0;
  int tCard = 0;
  int tMessage = 0;

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
    elapsedMillis diskTime;
    diskTime = 0;
    if(sectorCount < multiblock) {
      multiblock = sectorCount;
    }
    // Send it
    if(target_DOUT(TARGET_DISKBUF, 512, multiblock)) return 1;
    tSCSI += diskTime; diskTime = 0;

    // Write Sector
    DRESULT res = SDHC_WriteBlocks((UCHAR*) TARGET_DISKBUF, (DWORD) sectorOffset, multiblock);
    if(res != RES_OK) {
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = MEDIUM_ERROR;
      return 0;
    }
    SDHC_DMAWait();
    tCard += diskTime; diskTime = 0;

    // Check/Process Host Messages
    if(target_MessageProcess()) return 1;
    tMessage += diskTime; diskTime = 0;

    sectorOffset += multiblock;
    sectorCount -= multiblock;
  }

  DEBUGPRINT(DBG_TRACE, " S:%d/C:%d/M:%d ms ", tSCSI, tCard, tMessage);

  return 0;
}


// Group 0 Read Data
int target_CommandRead() {
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
int target_CommandWrite() {
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
int target_CommandGroup1Read() {
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
int target_CommandGroup1Write() {
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
int target_CommandGroup1SyncCache() {
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
int target_CommandGroup1ReadLong() {
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
int target_CommandGroup1WriteLong() {
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
int target_CommandFormat() {
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

