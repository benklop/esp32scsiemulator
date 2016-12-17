#include "SCSI_Commands.h"
#include "SCSI_Sense.h"
#include <SPI.h>
#include <EtherRaw.h>
#include <utility/w5100.h>

typedef struct ctron_stats {
  uint32_t  frames_xmit;      /* Frames Transmitted */
  uint32_t  bytes_xmit;
  uint32_t  ucast_xmit;      /* never incremented? */
  uint32_t  mcast_xmit;      /* gets ucasts and mcasts?? */
  uint32_t  bcast_xmit;
  uint32_t  defer_xmit;
  uint32_t  sgl_coll;
  uint32_t  multi_coll;
  uint32_t  tot_xmit_err;
  uint32_t  late_coll;
  uint32_t  excess_coll;
  uint32_t  int_err_xmit;
  uint32_t  carr_err;
  uint32_t  media_abort;
  uint32_t  frames_rec;
  uint32_t  bytes_rec;
  uint32_t  ucast_rec;     /* never incremented? */
  uint32_t  mcast_rec;     /* gets ucasts and mcasts?? */
  uint32_t  bcast_rec;
  uint32_t  tot_rec_err;
  uint32_t  too_long;
  uint32_t  too_short;
  uint32_t  align_err;
  uint32_t  crc_err;
  uint32_t  len_err;
  uint32_t  int_err_rec;
  uint32_t  sqe_err;
} ctron_stats_t;

ctron_stats_t EthernetStats;

uint8_t target_ether_pktbuf[16384];

static uint8_t TARGET_ETHERNET_INQUIRY_RESPONSE[] = {
  0x03, 0x00, 0x00, 0x00,
  0x4b, 0x00, 0x00, 0x18,
  // Vendor ID (8 Bytes)
  'C','A','B','L','E','T','R','N',
  // Product ID (16 Bytes)
  'E','A','4','1','2','/','1','4',
  '/','1','9',' ',' ','D','0','0',
  // Revision Number (4 Bytes)
  '1','.','0','0',
  // Firmware Version (8 Bytes)
  '0','1','.','0','0','.','0','0',
  // Hardware Address (Fixed, Current)
  0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xF1,
  0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0xF1,
  // Firmware Date [22]
  'M', 'o', 'n', ' ', 'J', 'u', 'n', ' ', ' ', '7',
  '1', '9', '9', '3', ' ', '0', '8', ':', '1', '7',
  // Media Type
  0,
  // HW Read Port
  0,
  // ????
  0
};

void etherInit() {
  W5100.init();
  SPI1.beginTransaction(SPI_ETHERNET_SETTINGS);
  W5100.setMACAddress(TARGET_ETHERNET_INQUIRY_RESPONSE+44);
  SPI1.endTransaction();
  W5100.writeSnMR(0, SnMR::MACRAW); 
  W5100.execCmdSn(0, Sock_OPEN);
  memset(&EthernetStats, 0, sizeof(EthernetStats));
}

// GetAddr Command
int target_EthernetGetAddr() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > GetAddr");
  
  // Setup response
  memcpy(TARGET_RESPBUF, lun[target_lun].HWAddr, 6);
  len = 6;
  SPI1.beginTransaction(SPI_ETHERNET_SETTINGS);
  W5100.getMACAddress(TARGET_RESPBUF);
  SPI1.endTransaction();

  // Truncate if necessary
  if((target_cmdbuf[4] > 0) && (target_cmdbuf[4] < len))
    len = target_cmdbuf[4];

  DEBUGPRINT(DBG_TRACE, " %d %02x:%02x:%02x:%02x:%02x:%02x", target_cmdbuf[4],
    TARGET_RESPBUF[0], TARGET_RESPBUF[1],
    TARGET_RESPBUF[2], TARGET_RESPBUF[3],
    TARGET_RESPBUF[4], TARGET_RESPBUF[5]);

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

  int rv = target_DOUT(TARGET_RESPBUF, len, 1);

  DEBUGPRINT(DBG_TRACE, " %d %02x:%02x:%02x:%02x:%02x:%02x", target_cmdbuf[4],
    TARGET_RESPBUF[0], TARGET_RESPBUF[1],
    TARGET_RESPBUF[2], TARGET_RESPBUF[3],
    TARGET_RESPBUF[4], TARGET_RESPBUF[5]);

  SPI1.beginTransaction(SPI_ETHERNET_SETTINGS);
  W5100.setMACAddress(TARGET_RESPBUF);
  SPI1.endTransaction();
  memcpy(lun[target_lun].HWAddr, TARGET_RESPBUF, len);

  return rv;
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

// GetStats Command
int target_EthernetGetStats() {
  uint8_t len;
  DEBUGPRINT(DBG_TRACE, " > GetStats");
  
  // Setup response
  memcpy(TARGET_RESPBUF, &EthernetStats, sizeof(EthernetStats));
  len = sizeof(EthernetStats);

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
  TARGET_RESPBUF[79] = target_lun >> 3;
  
  // Truncate if necessary
  if(target_cmdbuf[4] < len)
    len = target_cmdbuf[4];

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

int target_EthernetSend() {
  DEBUGPRINT(DBG_TRACE, " > NetSend");
  target_ether_pktbuf[0] = target_cmdbuf[3];
  target_ether_pktbuf[1] = target_cmdbuf[4];
  uint16_t len = (target_ether_pktbuf[0] << 8) | target_ether_pktbuf[1];
  int rv = target_DOUT(target_ether_pktbuf+2, len-2, 1);
  DEBUGPRINT(DBG_TRACE, " %02x%02x",
    target_ether_pktbuf[0], target_ether_pktbuf[1]);
  DEBUGPRINT(DBG_TRACE, " %02x:%02x:%02x:%02x:%02x:%02x", 
    target_ether_pktbuf[8], target_ether_pktbuf[9],
    target_ether_pktbuf[10], target_ether_pktbuf[11],
    target_ether_pktbuf[12], target_ether_pktbuf[13]);
  DEBUGPRINT(DBG_TRACE, "-> %02x:%02x:%02x:%02x:%02x:%02x", 
    target_ether_pktbuf[2], target_ether_pktbuf[3],
    target_ether_pktbuf[4], target_ether_pktbuf[5],
    target_ether_pktbuf[6], target_ether_pktbuf[7]);
  // Hand packet off to ethercon
  W5100.send_data_processing(0, target_ether_pktbuf, len);
  W5100.execCmdSn(0, Sock_SEND_MAC);
  EthernetStats.frames_xmit++;
  return rv;
}

int target_EthernetRecv() {
  DEBUGPRINT(DBG_TRACE, " > NetRecv");
  uint16_t len = W5100.getRXReceivedSize(0);
  if(len > 0) {
    // Get packet from ethercon
    W5100.recv_data_processing(0, target_ether_pktbuf, len);
    W5100.execCmdSn(0, Sock_RECV);
  DEBUGPRINT(DBG_TRACE, " %02x%02x",
    target_ether_pktbuf[0], target_ether_pktbuf[1]);
  DEBUGPRINT(DBG_TRACE, " %02x:%02x:%02x:%02x:%02x:%02x", 
    target_ether_pktbuf[8], target_ether_pktbuf[9],
    target_ether_pktbuf[10], target_ether_pktbuf[11],
    target_ether_pktbuf[12], target_ether_pktbuf[13]);
  DEBUGPRINT(DBG_TRACE, "-> %02x:%02x:%02x:%02x:%02x:%02x", 
    target_ether_pktbuf[2], target_ether_pktbuf[3],
    target_ether_pktbuf[4], target_ether_pktbuf[5],
    target_ether_pktbuf[6], target_ether_pktbuf[7]);
    len = (target_ether_pktbuf[0] << 8) | target_ether_pktbuf[1];
    EthernetStats.frames_rec++;
    return target_DIN(target_ether_pktbuf, len, 1);
  }
  return 0;
}

int target_EthernetAddProtocol() {
  DEBUGPRINT(DBG_TRACE, " > Add Protocol");
  uint16_t len = target_cmdbuf[4];
  int rv = target_DOUT(target_ether_pktbuf, len, 1);
  DEBUGPRINT(DBG_TRACE, " %d %02x%02x", target_cmdbuf[4], target_ether_pktbuf[0], target_ether_pktbuf[2]);
  return rv;
}

int target_EthernetRemoveProtocol() {
  DEBUGPRINT(DBG_TRACE, " > Remove Protocol");
  uint16_t len = target_cmdbuf[4];
  int rv = target_DOUT(target_ether_pktbuf, len, 1);
  DEBUGPRINT(DBG_TRACE, " %d %02x%02x", target_cmdbuf[4], target_ether_pktbuf[0], target_ether_pktbuf[1]);
  return rv;
}

int target_EthernetAddMulticast() {
  DEBUGPRINT(DBG_TRACE, " > Add Multicast");
  uint16_t len = target_cmdbuf[4];
  int rv = target_DOUT(target_ether_pktbuf, len, 1);
  DEBUGPRINT(DBG_TRACE, " %d %02x:%02x:%02x:%02x:%02x:%02x", target_cmdbuf[4],
    target_ether_pktbuf[0], target_ether_pktbuf[1],
    target_ether_pktbuf[2], target_ether_pktbuf[3],
    target_ether_pktbuf[4], target_ether_pktbuf[5]);
  return rv;
}

int target_EthernetRemoveMulticast() {
  DEBUGPRINT(DBG_TRACE, " > Remove Multicast");
  uint16_t len = target_cmdbuf[4];
  int rv = target_DOUT(target_ether_pktbuf, len, 1);
  DEBUGPRINT(DBG_TRACE, " %d %02x:%02x:%02x:%02x:%02x:%02x", target_cmdbuf[4],
    target_ether_pktbuf[0], target_ether_pktbuf[1],
    target_ether_pktbuf[2], target_ether_pktbuf[3],
    target_ether_pktbuf[4], target_ether_pktbuf[5]);
  return rv;
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
      return target_EthernetAddProtocol();
    case CMD_ETHER_REM_PROTO:
      return target_EthernetRemoveProtocol();
    case CMD_ETHER_SET_MODE:
      return target_EthernetSetMode();
    case CMD_ETHER_SET_MULTI:
      return target_EthernetAddMulticast();
    case CMD_ETHER_REMOVE_MULTI:
      return target_EthernetRemoveMulticast();
    case CMD_ETHER_GET_STATS:
	    return target_EthernetGetStats();
    case CMD_ETHER_SET_MEDIA:
      return target_EthernetSetMedia();
    case CMD_ETHER_GET_MEDIA:
      return target_EthernetGetMedia();
    case CMD_ETHER_LOAD_IMAGE:
      DEBUGPRINT(DBG_TRACE, " > LoadImage");
	    return -1;
    case CMD_ETHER_SET_ADDR:
	    return target_EthernetSetAddr();
    case CMD_ETHER_GET_ADDR:
	    return target_EthernetGetAddr();
  }
  return -1;
}
