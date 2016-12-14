#include "SCSI_Commands.h"
#include "SCSI_Sense.h"

uint8_t target_ether_pktbuf[1536];

static uint8_t TARGET_ETHERNET_INQUIRY_RESPONSE[] = {
  0x03, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x00, 0x18,
  // Vendor ID (8 Bytes)
  'C','a','b','l','e','t','r','n',
  // Product ID (16 Bytes)
  'E','A','4','1','2',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
  // Revision Number (4 Bytes)
  '0','1','0','0',
  // Serial Number (8 Bytes)
  '0','1','.','0','0','.','0','0',
  // Hardware Address (Fixed, Current)
  0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xF1,
  0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xF1,
  // Firmware Date [22]
  '1', '2', '/', '0', '3', '/', '2', '0', '1', '6',
  '1', '2', ':', '1', '8', 'A', 'M', 0x0, 0x0, 0x0,
  // Media Type
  0,
  // HW Read Port
  0
};

// GetAddr Command
int target_EthernetGetAddr() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > GetAddr");
  
  // Setup response
  memcpy(TARGET_RESPBUF, lun[target_lun].HWAddr, 6);
  len = 6;

  // Truncate if necessary
  if((target_cmdbuf[4] > 0) && (target_cmdbuf[4] < len))
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

// SetAddr Command
int target_EthernetSetAddr() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > SetAddr");

  len = 6;
  if((target_cmdbuf[4] > 0) && (target_cmdbuf[4] < len))
    len = target_cmdbuf[4];

  target_DOUT(TARGET_RESPBUF, len, 1);

  memcpy(lun[target_lun].HWAddr, TARGET_RESPBUF, len);

  return 0;
}

// GetMedia Command
int target_EthernetGetMedia() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > GetMedia");
  
  // Setup response
  TARGET_RESPBUF[0] = lun[target_lun].MediaType;
  len = 1;

  // Truncate if necessary
  if((target_cmdbuf[4] > 0) && (target_cmdbuf[4] < len))
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

// SetMedia Command
int target_EthernetSetMedia() {
  DEBUGPRINT(DBG_TRACE, " > SetMedia");

  lun[target_lun].MediaType = target_cmdbuf[2];
  
  return 0;
}

// SetMode Command
int target_EthernetSetMode() {
  DEBUGPRINT(DBG_TRACE, " > SetMode");

  return 0;
}

// Inquiry Command
int target_EthernetInquiry() {
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

  TARGET_RESPBUF[78] = lun[target_lun].MediaType;
  
  // Truncate if necessary
  if(target_cmdbuf[4] < len)
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

int target_EthernetSend() {
  int rv = target_DOUT(target_ether_pktbuf, sizeof(target_ether_pktbuf), 1);
  // Hand packet off to ethercon
  return rv;
}

int target_EthernetRecv() {
  // Get packet from ethercon
  return target_DIN(target_ether_pktbuf, sizeof(target_ether_pktbuf), 1);
}

int target_EthernetProcess() {
  uint16_t ethercmd = (target_cmdbuf[0] << 8) | target_cmdbuf[1];

  switch(target_cmdbuf[0]) {
    case CMD_ETHER_RECV:
      return target_EthernetRecv();
  }
  switch(ethercmd) {
    case CMD_ETHER_SEND:
      return target_EthernetSend();
    case CMD_ETHER_ADD_PROTO:
    case CMD_ETHER_REM_PROTO:
      // Add / Remove Protocols from the Frame Filter
	    return 0;
    case CMD_ETHER_SET_MODE:
      return target_EthernetSetMode();
    case CMD_ETHER_SET_MULTI:
    case CMD_ETHER_REMOVE_MULTI:
      // Add / Remove Multicast from the Frame Filter
  	  return 0;
    case CMD_ETHER_GET_STATS:
	    return 0;
    case CMD_ETHER_SET_MEDIA:
      return target_EthernetSetMedia();
    case CMD_ETHER_GET_MEDIA:
      return target_EthernetGetMedia();
    case CMD_ETHER_LOAD_IMAGE:
	    return 0;
    case CMD_ETHER_SET_ADDR:
	    return target_EthernetSetAddr();
    case CMD_ETHER_GET_ADDR:
	    return target_EthernetGetAddr();
  }
  return -1;
}
