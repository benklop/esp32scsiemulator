#include "config.h"
#ifdef SUPPORT_ETHERNET_SCSILINK
#include "scsiCommands.h"
#include "scsiSense.h"
#include "svcEthernet.h"

// We don't need to, but we can prepare for Jumbo frames.
uint8_t target_ether_scsibuf[10240];

typedef struct SCSILinkStats_s {
  uint8_t mac_address[6];
  uint8_t frame_alignment_errors[4];
  uint8_t crc_errors[4];
  uint8_t frames_lost[4];
} SCSILinkStats_t;

SCSILinkStats_t SCSILinkStats = {
  { 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x01 },
  { 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x00, 0x00 }
};

static uint8_t TARGET_ETHERNET_INQUIRY_RESPONSE[] = {
  0x03, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x00, 0x18,
  // Vendor ID (8 Bytes)
  'D','A','Y','N','A','T','R','N',
  // Product ID (16 Bytes)
  'S','C','S','I','/','L','i','n',
  'k','-','T',' ',' ',' ',' ',' ',
  // Revision Number (4 Bytes)
  '1','.','0','0',
  // Firmware Version (8 Bytes)
  '0','1','.','0','0','.','0','0',
};

// SetAddr Command
static int target_SCSILinkSetAddr() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > SetAddr");

  len = 6;
  int rv = target_DOUT(TARGET_RESPBUF, len, 1);

  DEBUGPRINT(DBG_TRACE, " %d %02x:%02x:%02x:%02x:%02x:%02x", target_cmdbuf[4],
    TARGET_RESPBUF[0], TARGET_RESPBUF[1],
    TARGET_RESPBUF[2], TARGET_RESPBUF[3],
    TARGET_RESPBUF[4], TARGET_RESPBUF[5]);

  ethernetSetMAC(TARGET_RESPBUF);

  memcpy(SCSILinkStats.mac_address, TARGET_RESPBUF, sizeof(SCSILinkStats.mac_address));

  return rv;
}

// GetStats Command
static int target_SCSILinkGetStats() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > GetStats");
  
  // Setup response
  memcpy(TARGET_RESPBUF, &SCSILinkStats, sizeof(SCSILinkStats));
  len = sizeof(SCSILinkStats);

  // Truncate if necessary
  if((target_cmdbuf[4] > 0) && (target_cmdbuf[4] < len))
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

// SetMedia Command
static int target_SCSILinkEnable() {
  DEBUGPRINT(DBG_TRACE, " > Enable");

//  lun[target_lun].MediaType = target_cmdbuf[2];
  
  return 0;
}

// SetMode Command
static int target_SCSILinkSet() {
  DEBUGPRINT(DBG_TRACE, " > Set");

  switch(target_cmdbuf[5]) {
    case CMD_SCSILINK_SETMODE:
      return 0;
    case CMD_SCSILINK_SETMAC:
      return target_SCSILinkSetAddr();
  }

  target_status = STATUS_CHECK;
  target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
  target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
  target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 5
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x05;
  return 0;
}

// Inquiry Command
static int target_SCSILinkInquiry() {
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
  
  // Setup response
  memcpy(TARGET_RESPBUF, TARGET_ETHERNET_INQUIRY_RESPONSE, sizeof(TARGET_INQUIRY_RESPONSE));
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

static int target_SCSILinkSend() {
  DEBUGPRINT(DBG_TRACE, " > NetSend");
  uint16_t len = (target_cmdbuf[3] << 8) | target_cmdbuf[4];
  int pktoffset = 0;
  if(target_cmdbuf[5] & 0x7f) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 1
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x05;
    return 0;
  }
  int rv = target_DOUT(target_ether_scsibuf, len, 1);
  if(rv == 0) {
    switch(target_cmdbuf[5]) {
      case 0x80: // Padded/aligned packet (LongWord DMA?)
        pktoffset = 2;
        len = ((target_ether_scsibuf[0] << 8) | target_ether_scsibuf[1]) + 2;
        break;
      case 0x00: // Unaligned packet (ShortWord DMA?)
        pktoffset = 0;
        break;
    }

    // Push packet to output ring buffer
    target_EthernetSend(target_ether_scsibuf+pktoffset, len);

    DEBUGPRINT(DBG_TRACE, " %04x", len);
    DEBUGPRINT(DBG_TRACE, " %02x:%02x:%02x:%02x:%02x:%02x", 
      target_ether_scsibuf[pktoffset + 6], target_ether_scsibuf[pktoffset + 7],
      target_ether_scsibuf[pktoffset + 8], target_ether_scsibuf[pktoffset + 9],
      target_ether_scsibuf[pktoffset + 10], target_ether_scsibuf[pktoffset + 11]);
    DEBUGPRINT(DBG_TRACE, "-> %02x:%02x:%02x:%02x:%02x:%02x", 
      target_ether_scsibuf[pktoffset + 0], target_ether_scsibuf[pktoffset + 1],
      target_ether_scsibuf[pktoffset + 2], target_ether_scsibuf[pktoffset + 3],
      target_ether_scsibuf[pktoffset + 4], target_ether_scsibuf[pktoffset + 5]);
  }
  return rv;
}

static int target_SCSILinkRecv() {
  DEBUGPRINT(DBG_TRACE, " > NetRecv");

  int len = target_EthernetRecv(target_ether_scsibuf+6, sizeof(target_ether_scsibuf)-6);
  if(len > 0) {
    DEBUGPRINT(DBG_TRACE, " %04x", len);
    DEBUGPRINT(DBG_TRACE, " %02x:%02x:%02x:%02x:%02x:%02x", 
      target_ether_scsibuf[12], target_ether_scsibuf[13],
      target_ether_scsibuf[14], target_ether_scsibuf[15],
      target_ether_scsibuf[16], target_ether_scsibuf[17]);
    DEBUGPRINT(DBG_TRACE, "-> %02x:%02x:%02x:%02x:%02x:%02x", 
      target_ether_scsibuf[6], target_ether_scsibuf[7],
      target_ether_scsibuf[8], target_ether_scsibuf[9],
      target_ether_scsibuf[10], target_ether_scsibuf[11]);

    return target_DIN(target_ether_scsibuf, len, 1);
  }
  return 0;
}

int target_SCSILinkProcess() {
  switch(target_cmdbuf[0]) {
    case CMD_READ6:
      return target_SCSILinkRecv();
    case CMD_WRITE6:
      return target_SCSILinkSend();
    case CMD_SCSILINK_ENABLE:
      return target_SCSILinkEnable();
    case CMD_SCSILINK_SET:
      return target_SCSILinkSet();
    case CMD_SCSILINK_STATS:
	    return target_SCSILinkGetStats();
  }
  return -1;
}

#endif

