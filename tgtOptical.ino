#include "config.h"
#include "scsiCommands.h"
#include "scsiSense.h"
#include "LUNS.h"

static uint8_t TARGET_OPTICAL_INQUIRY_RESPONSE[96] = {
  0x00, 0x00, 0x01, 0x01,
  0x32, 0x00, 0x00, 0x00,
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

// Read Updated Block
int target_CommandReadUpdatedBlock() {
  uint32_t sectorOffset;
  uint32_t sectorCount;

  DEBUGPRINT(DBG_GENERAL, "\n\r\t>Read Updated Block(10)");
#if 0
  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = 1;

  // Read Sectors
  return ncrReadSectors(sectorOffset, sectorCount);
#endif
  return 0;
}

