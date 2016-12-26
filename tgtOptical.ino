#include "config.h"
#include "scsiCommands.h"
#include "scsiSense.h"
#include "LUNS.h"

static uint8_t TARGET_OPTICAL_INQUIRY_RESPONSE[96] = {
  0x05, 0x80, 0x05, 0x02,
  0x2a, 0x00, 0x00, 0x00,
  // Vendor ID (8 Bytes)
  'T','i','n','y','S','C','S','I',
  // Product ID (16 Bytes)
  'T','S','E','C','D','R','O','M',' ',' ',' ',' ',' ',' ',' ',' ',
  // Revision Number (4 Bytes)
  '0','0','0','0',
  // Release Number (1 Byte)
  0x20,
  // Revision Date (10 Bytes)
  '2','0','1','6','/','1','2','/','2','6',
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
  if(!lun[target_lun].Enabled) {
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

  // Truncate if necessary
  if(target_cmdbuf[4] < len)
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

int target_OpticalModeSense6() {
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
  TARGET_RESPBUF[len++] = 0x80;

  /* Add block descriptor if DBD is not set */
  if (target_cmdbuf[1] & 0x08) {
    TARGET_RESPBUF[len++] = 0;             /* No block descriptor */
  } else {
    uint32_t capacity = ((lun[target_lun].Size * 512) / lun[target_lun].SectorSize) - 1;
    TARGET_RESPBUF[len++] = 8;             /* Block descriptor length */
    TARGET_RESPBUF[len++] = (capacity >> 24) & 0xff;
    TARGET_RESPBUF[len++] = (capacity >> 16) & 0xff;
    TARGET_RESPBUF[len++] = (capacity >> 8) & 0xff;
    TARGET_RESPBUF[len++] = capacity & 0xff;
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
#if 0
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
#endif
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
  uint32_t capacity = ((lun[target_lun].Size * 512) / lun[target_lun].SectorSize) - 1;
  DEBUGPRINT(DBG_TRACE, " > Capacity");
  if(target_CheckMedium()) return 0;

  TARGET_RESPBUF[0] = (capacity >> 24) & 0xff;
  TARGET_RESPBUF[1] = (capacity >> 16) & 0xff;
  TARGET_RESPBUF[2] = (capacity >> 8) & 0xff;
  TARGET_RESPBUF[3] = capacity & 0xff;

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


static const uint8_t SessionTOC[] =
{
  0x00, // toc length, MSB
  0x0A, // toc length, LSB
  0x01, // First session number
  0x01, // Last session number,
  // TRACK 1 Descriptor
  0x00, // reserved
  0x14, // Q sub-channel encodes current position, Digital track
  0x01, // First track number in last complete session
  0x00, // Reserved
  0x00,0x00,0x00,0x00 // LBA of first track in last session
};


static const uint8_t FullTOC[] =
{
  0x00, // toc length, MSB
  0x44, // toc length, LSB
  0x01, // First session number
  0x01, // Last session number,
  // A0 Descriptor
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0xA0, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x01, // First Track number.
  0x00, // Disc type 00 = Mode 1
  0x00,  // PFRAME
  // A1
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0xA1, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x01, // Last Track number
  0x00, // PSEC
  0x00,  // PFRAME
  // A2
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0xA2, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x79, // LEADOUT position BCD
  0x59, // leadout PSEC BCD
  0x74, // leadout PFRAME BCD
  // TRACK 1 Descriptor
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0x01, // Point
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x00, // PMIN
  0x00, // PSEC
  0x00,  // PFRAME
  // b0
  0x01, // session number
  0x54, // ADR/Control
  0x00, // TNO
  0xB1, // POINT
  0x79, // Min BCD
  0x59, // Sec BCD
  0x74, // Frame BCD
  0x00, // Zero
  0x79, // PMIN BCD
  0x59, // PSEC BCD
  0x74,  // PFRAME BCD
  // c0
  0x01, // session number
  0x54, // ADR/Control
  0x00, // TNO
  0xC0, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x00, // PMIN
  0x00, // PSEC
  0x00  // PFRAME
};



static const uint8_t DiscInfoBlock[] =
{
  0x00, // disc info length, MSB
  0x44, // disc info length, LSB
  0x0e, // DiscStatus = Complete
  0x01, // First Track on Disc
  0x01, // Sessions on Disc (LSB)
  0x01, // First Track in Last Session (LSB)
  0x01, // Last Track in Last Session (LSB)
  0x20, // DID_V DBC_V URU
  0x00, // Disc Type
  0x00, // Sessions on Disc (MSB)
  0x00, // First Track in Last Session (MSB)
  0x00, // Last Track in Last Session (MSB)
  0x00, // DiscID (MSB)
  0x00, // DiscID
  0x00, // DiscID
  0x00, // DiscID (LSB)
  0x00, // Last Session Lead-In Start (MSB)
  0x00, //   IN MSF
  0x00, //
  0x00, // Last Session Lead-In Start (LSB)
  0x00, // Last Possible Lead-Out Start (MSB)
  0x00, //   IN MSF
  0x00, //
  0x00, // Last Possible Lead-Out Start (LSB)
  0x00, // Bar Code (MSB)
  0x00, //
  0x00, //
  0x00, //
  0x00, //
  0x00, //
  0x00, //
  0x00, // Bar Code (LSB)
  0x00, // Reserved
  0x00, // Number of OPC Entries
};

static void LBA2MSF(uint32_t LBA, uint8_t* MSF)
{
  MSF[0] = 0; // reserved.
  MSF[3] = (uint32_t)(LBA % 75); // F
  uint32_t rem = LBA / 75;

  MSF[2] = (uint32_t)(rem % 60); // S
  MSF[1] = (uint32_t)(rem / 60); // M

}

static int target_OpticalReadSimpleTOC()
{
  int MSF = target_cmdbuf[1] & 0x02 ? 1 : 0;
  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  uint32_t len = 0;
  memset(TARGET_RESPBUF, 0, sizeof(TARGET_RESPBUF));

  uint32_t capacity = (uint32_t)(lun[target_lun].Size * 512) / (uint32_t)(lun[target_lun].SectorSize);
  TARGET_RESPBUF[len++] = 0x00; // toc length, MSB
  TARGET_RESPBUF[len++] = 0x00; // toc length, LSB
  TARGET_RESPBUF[len++] = 0x01; // First track number
  TARGET_RESPBUF[len++] = 0x01; // Last track number

  // We only support track 1 and 0xaa (lead-out).
  // track 0 means "return all tracks"
  switch (target_cmdbuf[6]) {
    case 0x00:
    case 0x01:
      TARGET_RESPBUF[len++] = 0x00; // reserved
      TARGET_RESPBUF[len++] = 0x14; // Q sub-channel encodes current position, Digital track
      TARGET_RESPBUF[len++] = 0x01; // Track 1
      TARGET_RESPBUF[len++] = 0x00; // Reserved
      TARGET_RESPBUF[len++] = 0x00; // MSB LBA
      TARGET_RESPBUF[len++] = 0x00; // 
      TARGET_RESPBUF[len++] = 0x00; // 
      TARGET_RESPBUF[len++] = 0x00; // LSB LBA
    case 0xAA:
      TARGET_RESPBUF[len++] = 0x00; // reserved
      TARGET_RESPBUF[len++] = 0x14; // Q sub-channel encodes current position, Digital track
      TARGET_RESPBUF[len++] = 0xAA; // Leadout Track
      TARGET_RESPBUF[len++] = 0x00; // Reserved
      // Replace start of leadout track
      if (MSF)
      {
        LBA2MSF(capacity, TARGET_RESPBUF + len);
        len+=4;
      }
      else
      {
        // Track start sector (LBA)
        TARGET_RESPBUF[len++] = capacity >> 24;
        TARGET_RESPBUF[len++] = capacity >> 16;
        TARGET_RESPBUF[len++] = capacity >> 8;
        TARGET_RESPBUF[len++] = capacity;
      }
      break;
    default:
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
      target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
      target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 6
      target_sense[target_lun].key_specific[1] = 0x00;
      target_sense[target_lun].key_specific[2] = 0x06;
      return 0;
  }

  TARGET_RESPBUF[0] = (len >> 8) & 0xff;
  TARGET_RESPBUF[1] = len & 0xff;
  
  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

static int target_OpticalReadSessionInfo()
{
//  int MSF = target_cmdbuf[1] & 0x02 ? 1 : 0;
  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];
  uint32_t len = sizeof(SessionTOC);
  memcpy(TARGET_RESPBUF, SessionTOC, len);

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

static inline uint8_t
fromBCD(uint8_t val)
{
  return ((val >> 4) * 10) + (val & 0xF);
}

static int target_OpticalReadFullTOC(int convertBCD)
{
  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];
  // We only support session 1.
  if (target_cmdbuf[6] > 1) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 6
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x06;
    return 0;
  }

  uint32_t len = sizeof(FullTOC);
  memcpy(TARGET_RESPBUF, FullTOC, len);

  if (convertBCD)
  {
    uint32_t descriptor = 4;
    while (descriptor < len)
    {
      int i;
      for (i = 0; i < 7; ++i)
      {
        TARGET_RESPBUF[descriptor + i] =
          fromBCD(TARGET_RESPBUF[descriptor + 4 + i]);
      }
      descriptor += 11;
    }

  }

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

static int target_OpticalReadTOC()
{
  DEBUGPRINT(DBG_TRACE, " > Read TOC");
  switch(target_cmdbuf[2] & 0xf) {
    case 0:
      DEBUGPRINT(DBG_TRACE, " Simple");
      return target_OpticalReadSimpleTOC();
    case 1:
      DEBUGPRINT(DBG_TRACE, " Session");
      return target_OpticalReadSessionInfo();
    case 2:
      DEBUGPRINT(DBG_TRACE, " Full 0");
      return target_OpticalReadFullTOC(0);
    case 3:
      DEBUGPRINT(DBG_TRACE, " Full 1");
      return target_OpticalReadFullTOC(1);
  }

  target_status = STATUS_CHECK;
  target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
  target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
  target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 2
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x02;
  return 0;
}

static uint8_t SimpleHeader[] =
{
  0x01, // 2048byte user data, L-EC in 288 byte aux field.
  0x00, // reserved
  0x00, // reserved
  0x00, // reserved
  0x00,0x00,0x00,0x00 // Track start sector (LBA or MSF)
};

static int target_OpticalHeader()
{
  uint32_t len = sizeof(SimpleHeader);
  memcpy(TARGET_RESPBUF, SimpleHeader, len);

  DEBUGPRINT(DBG_TRACE, " > Read Header");

  //int MSF = target_cmdbuf[1] & 0x02 ? 1 : 0;
  //uint32_t lba = 0;
  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}


static int target_OpticalReadDiscInfo()
{
  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];
  uint32_t len = sizeof(DiscInfoBlock);
  memcpy(TARGET_RESPBUF, DiscInfoBlock, len);

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

uint16_t SupportedFeatures[] = {
  0x0001, // Core Features
  0x0003, // Removable Media
  0x0010, // Random Readable
  0x001D, // Reads all Media Types
  0x001E, // Reads CD Structures
};

static int target_OpticalGetConfiguration()
{
  uint16_t sfn = (target_cmdbuf[2] << 8) | target_cmdbuf[3];
  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];
  uint16_t sfnmax = (allocationLength - 8) / 4;
  uint16_t sfi = 0;
  uint16_t len;

  memset(TARGET_RESPBUF, 0, sizeof(TARGET_RESPBUF));

  switch(target_cmdbuf[1] & 0x3) {
    case 0: {
      break;
    }
    case 1: {
      break;
    }
    case 2: {
      sfnmax = 1;
      break;
    }
  }

  len = 8;
  for(sfi = 0; (sfi < sizeof(SupportedFeatures)) && sfnmax; sfi++) {
    if( SupportedFeatures[sfi] > sfn ) {
      TARGET_RESPBUF[len++] = (SupportedFeatures[sfi] >> 8) & 0xff;
      TARGET_RESPBUF[len++] = SupportedFeatures[sfi] & 0xff;
      TARGET_RESPBUF[len++] = 0;
      TARGET_RESPBUF[len++] = 0;
      sfnmax--;
    }
  }

  TARGET_RESPBUF[0] = (len >> 24) & 0xff;
  TARGET_RESPBUF[1] = (len >> 16) & 0xff;
  TARGET_RESPBUF[2] = (len >> 8) & 0xff;
  TARGET_RESPBUF[3] = (len) & 0xff;

  TARGET_RESPBUF[6] = 0x00; // CD-ROM Profile
  TARGET_RESPBUF[7] = 0x08;
  
  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}


static int target_OpticalEventStatus()
{
  uint32_t len = 0;
  memset(TARGET_RESPBUF, 0, sizeof(TARGET_RESPBUF));

  DEBUGPRINT(DBG_TRACE, " > Read Event Status");

  if((target_cmdbuf[1] & 1) == 0) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 1
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x01;
    return 0;
  }

  int ncr;
  for(ncr = 0; ncr < 8; ncr++) {
    if(target_cmdbuf[4] & (1<<ncr)) {
      switch (ncr) {
        case 1:
          TARGET_RESPBUF[len++] = 0x00; // MSB Descriptor Length
          TARGET_RESPBUF[len++] = 0x06; // LSB
          TARGET_RESPBUF[len++] = 0x02; // Operational Change Event
          TARGET_RESPBUF[len++] = 0x12; // Supported Event Class
          TARGET_RESPBUF[len++] = 0x00; // No Change
          TARGET_RESPBUF[len++] = 0x00; // Operational Status
          TARGET_RESPBUF[len++] = 0x00; // MSB Operational Change
          TARGET_RESPBUF[len++] = 0x00; // LSB
          break;
        case 4:
          TARGET_RESPBUF[len++] = 0x00; // MSB Descriptor Length
          TARGET_RESPBUF[len++] = 0x06; // LSB
          TARGET_RESPBUF[len++] = 0x04; // Media Change Event
          TARGET_RESPBUF[len++] = 0x12; // Supported Event Class
          TARGET_RESPBUF[len++] = 0x00; // No Change
          TARGET_RESPBUF[len++] = lun[target_lun].Mounted ? 0x02 : 0x00; // Media Present
          TARGET_RESPBUF[len++] = 0x00; // Start Slot
          TARGET_RESPBUF[len++] = 0x00; // End Slot
          break;
      }
    }
  }

  uint16_t allocationLength = (target_cmdbuf[7] << 8) | target_cmdbuf[8];

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

static int target_OpticalLockTray() {
  if(target_cmdbuf[4] & 1) {
    DEBUGPRINT(DBG_TRACE, " > Lock Tray");
  } else {
    DEBUGPRINT(DBG_TRACE, " > Unlock Tray");
  }

  return 0;
}

int target_OpticalProcess() {
    switch(target_cmdbuf[0]) {
      case CMD_TEST_UNIT_READY:     return target_CommandTestReady();
      case CMD_PREVENT_REMOVAL:     return target_OpticalLockTray();
      case CMD_REZERO_UNIT:         return target_DiskReZero();
      case CMD_READ6:               return target_DiskRead();
      case CMD_MODE_SENSE6:         return target_OpticalModeSense6();
      case CMD_START_STOP_UNIT:     return target_CommandStartStop();
      case CMD_SEND_DIAGNOSTIC:     return target_CommandSendDiag();
      case CMD_READ_CAPACITY10:     return target_OpticalCapacity();
      case CMD_GET_CONFIGURATION:   return target_OpticalGetConfiguration();
      case CMD_READ_TOC:            return target_OpticalReadTOC();
      case CMD_READ_HEADER:         return target_OpticalHeader();
      case CMD_GET_EVENT_STATUS_NOTIFICATION: return target_OpticalEventStatus();
      case CMD_READ_DISC_INFORMATION: return target_OpticalReadDiscInfo();
      case CMD_READ10:              return target_DiskGroup1Read();
      case CMD_READLONG10:          return target_DiskGroup1ReadLong();
      case CMD_READUPDATEDBLOCK10:  return target_CommandReadUpdatedBlock();
    }
    return -1;
}

