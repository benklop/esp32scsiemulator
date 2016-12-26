#include "config.h"
#include "scsiMessages.h"
#include "scsiCommands.h"
#include "scsiSense.h"
#include "DebugConsole.h"
#include "drvNCR5380.h"
#include "LUNS.h"
#include "svcTarget.h"

#define PHASE_INVALID 0
#define PHASE_SELECT 1

static uint8_t target_msg = 0;
static uint8_t target_phase = PHASE_INVALID;
int target_interrupt = 1;
int target_initialized = 0;

SenseData_t target_sense[MAXLUNS];

uint8_t unit_attention = 0xff;
static uint8_t unit_attention_queue[8];

#define UA_POTR   0x01
#define UA_NRTRC  0x02

static uint8_t target_status = STATUS_GOOD;
static uint8_t target_lun = 0xff;
static uint8_t target_id = 0xff;
static uint8_t target_idmask = 0x02;
static uint8_t target_cmdlen = 0;
static uint8_t target_cmdbuf[16];
static uint8_t TARGET_RESPBUF[256];

static uint8_t target_initiator = 0;

void print_cdb() {
  DEBUGPRINT(DBG_GENERAL, " [");
  for(int ii=0; ii<target_cmdlen; ii++) {
    DEBUGPRINT(DBG_GENERAL, " %02x,", target_cmdbuf[ii]);
  }
  DEBUGPRINT(DBG_GENERAL, " ]");
}

int target_GetMessageOut() {
  int rv;
  PhaseDelay();
  ncr_WriteRegister(TARGET_COMMAND_REG, (TCR_ASSERT_MSG | TCR_ASSERT_CD));
  rv=ncr_GetPIO(&target_msg, 1, 1);
  return rv;
}

int target_PutMessageIn() {
  int rv;
  PhaseDelay();
  ncr_WriteRegister(TARGET_COMMAND_REG, (TCR_ASSERT_MSG | TCR_ASSERT_CD | TCR_ASSERT_IO));
  rv= ncr_PutPIO(&target_msg, 1, 1);
  return rv;
}

// Transfer command from initiator to target
int target_GetCommand() {
  int len = -1;
  int rv = 0;

//  DEBUGPRINT(DBG_TRACE, " > Command: [");
  PhaseDelay();

  ncr_WriteRegister(TARGET_COMMAND_REG, TCR_ASSERT_CD);
  target_cmdbuf[0] = 0xAA;
  ncr_GetPIO(target_cmdbuf, 1, 1);

  switch(target_cmdbuf[0] >> 5) {
    case 0: len = 5; break;
    case 1:
    case 2: len = 9; break;
    case 4: len = 15; break;
    case 5: len = 11; break;
    case 7: len = 0; break;
  }

//  DEBUGPRINT(DBG_TRACE, " %02x", target_cmdbuf[0]);

  if(len >= 0) {
    if(len > 0)
      rv=ncr_GetPIO(target_cmdbuf+1, len, 1);
  
    target_cmdlen = len+1;
//    for(int ii=1; ii<target_cmdlen; ii++) {
//      DEBUGPRINT(DBG_LOWLEVEL, ", %02x", target_cmdbuf[ii]);
//    }
//    DEBUGPRINT(DBG_LOWLEVEL, " ]");
    return rv;
  }

  DEBUGPRINT(DBG_WARN, " > Unknown Command/Length");
  target_cmdlen=1;
  print_cdb();
  target_cmdlen=0;
  target_status = STATUS_CHECK;
  target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
  target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
  target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in opcode
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x00;

  return 0;
}

// Transfer status from target to initiator
int target_PutStatus() {
  DEBUGPRINT(DBG_TRACE, " > Status: %d", target_status);
  PhaseDelay();

  ncr_WriteRegister(TARGET_COMMAND_REG, (TCR_ASSERT_CD | TCR_ASSERT_IO));
  return ncr_PutPIO(&target_status, 1, 1);
}

// Data Out Phase initiator to target
int target_DOUT(uint8_t *buf, uint16_t len, uint16_t count) {
  ncr_WriteRegister(TARGET_COMMAND_REG, 0);
  return ncr_Get(buf, len, count);
}

// Data In Phase target to initiator
int target_DIN(uint8_t *buf, uint16_t len, uint16_t count) {
  ncr_WriteRegister(TARGET_COMMAND_REG, TCR_ASSERT_IO);
  return ncr_Put(buf, len, count);
}

uint8_t TARGET_EXTMSG[256];

// Check for initiator Messages and Handle Them
int target_MessageProcess() {
  DEBUGPRINT(DBG_LOWLEVEL, " > MsgProc");

  elapsedMillis msgTimeout;

  // poll for ATN flag
  while((ncr_ReadRegister(BUS_AND_STATUS_REG) & BASR_ATN) == BASR_ATN) {
    if(msgTimeout > 1000) return 1; // Prevent deadlocks
    if(target_GetMessageOut()) return 1;
    if(target_msg & 0x80) {
      // target_MessageIdentify();
      target_lun = (target_id<<3) | (target_msg & 0x07);
//      DEBUGPRINT(DBG_LOWLEVEL, " > Select LUN: %d", target_lun);
    } else if(target_msg == 1) {
      uint16_t MSG_LEN;
      uint16_t MSG_PTR;
      uint8_t MSG_CODE;
//      DEBUGPRINT(DBG_TRACE, " > Extended Message %d", target_msg);
      if(target_GetMessageOut()) return 1;
      TARGET_EXTMSG[0] = MSG_LEN = target_msg;
      if(MSG_LEN == 0) MSG_LEN = 256;
//      DEBUGPRINT(DBG_TRACE, " %d", MSG_LEN);
      if(target_GetMessageOut()) return 1;
      TARGET_EXTMSG[1] = MSG_CODE = target_msg;
//      DEBUGPRINT(DBG_TRACE, " %d", MSG_CODE);
      MSG_PTR=2;
      while(MSG_PTR < MSG_LEN) {
        if(target_GetMessageOut()) return 1;
        TARGET_EXTMSG[MSG_PTR++] = target_msg;
//        DEBUGPRINT(DBG_TRACE, " %d", target_msg);
      }
      target_msg = 0x07;
      if(target_PutMessageIn()) return 1;
    } else {
      DEBUGPRINT(DBG_WARN, " > Unknown Message %d", target_msg);
      // Reject Unknown Message
      target_msg = 0x07;
      if(target_PutMessageIn()) return 1;
    }
  }
  return 0;
}

int target_CheckMedium() {
  if(!lun[target_lun].Enabled) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = NOT_READY;
    target_sense[target_lun].code = 0x3A;          /* "Medium not present" */
    DEBUGPRINT(DBG_WARN, " > Medium not present");
    return 1;
  }
  if(!lun[target_lun].Mounted) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = NOT_READY;
    target_sense[target_lun].code = 0x4;          /* "LUN not ready" */
    target_sense[target_lun].qualifier = 0x02;    /* "Initializing command required" */
    DEBUGPRINT(DBG_WARN, " > LUN not ready");
    return 1;
  }
  return 0;
}

// Test Unit Ready - Returns with Good Status
int target_CommandTestReady() {
  DEBUGPRINT(DBG_TRACE, " > Test Unit Ready");
  if(target_CheckMedium()) return 0;
  return 0;
}

// Sense
int target_CommandSense() {
  uint8_t len = 18;
  DEBUGPRINT(DBG_TRACE, " > Sense: %d", target_sense[target_lun].key);
  // Setup sense response
  TARGET_RESPBUF[0] = 0x70 | (1<<7); // Valid Fixed Sense Response
  TARGET_RESPBUF[1] = 0x00;
  TARGET_RESPBUF[2] = target_sense[target_lun].key & 0x0f;
  TARGET_RESPBUF[3] = target_sense[target_lun].info[0];
  TARGET_RESPBUF[4] = target_sense[target_lun].info[1];
  TARGET_RESPBUF[5] = target_sense[target_lun].info[2];
  TARGET_RESPBUF[6] = target_sense[target_lun].info[3];
  TARGET_RESPBUF[7] = 0x0A; // Additional Length
  TARGET_RESPBUF[8] = 0x00;
  TARGET_RESPBUF[9] = 0x00;
  TARGET_RESPBUF[10] = 0x00;
  TARGET_RESPBUF[11] = 0x00;
  TARGET_RESPBUF[12] = target_sense[target_lun].code;
  TARGET_RESPBUF[13] = target_sense[target_lun].qualifier;
  TARGET_RESPBUF[14] = 0x00;
  TARGET_RESPBUF[15] = target_sense[target_lun].key_specific[0];
  TARGET_RESPBUF[16] = target_sense[target_lun].key_specific[1];
  TARGET_RESPBUF[17] = target_sense[target_lun].key_specific[2];

  if(target_cmdbuf[4] == 0) {
    len = 4;
  } else if(target_cmdbuf[4] < len) {
    len = target_cmdbuf[4];
    TARGET_RESPBUF[7] = len - 8; // Additional Length
  }

  // Clear Sense Data
  target_sense[target_lun].key = NO_SENSE;
  target_sense[target_lun].code = NO_ADDITIONAL_SENSE_INFORMATION;
  target_sense[target_lun].qualifier = 0x00;
  target_sense[target_lun].info[0] = 0x00;
  target_sense[target_lun].info[1] = 0x00;
  target_sense[target_lun].info[2] = 0x00;
  target_sense[target_lun].info[3] = 0x00;
  target_sense[target_lun].key_specific[0] = 0x00;
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x00;

  // Send it
  return target_DIN(TARGET_RESPBUF, len, 1);
}

//  Start / Stop Unit Command
int target_CommandStartStop() {
  DEBUGPRINT(DBG_TRACE, " > Start/Stop");
  if(!luns) {
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = NOT_READY;
    target_sense[target_lun].code = 0x3A;          /* "Medium not present" */
    return 0;
  }
  if(!(target_cmdbuf[4] & 0xF0)) {
    if(target_cmdbuf[4] & 0x01) {
      DEBUGPRINT(DBG_TRACE, " Mount");
      lun[target_lun].Mounted = 1;
    } else {
      DEBUGPRINT(DBG_TRACE, " Dismount");
      lun[target_lun].Mounted = 0;
    }
  }

  return 0;
}

//  Send Diagostics Command
int target_CommandSendDiag() {
  DEBUGPRINT(DBG_TRACE, " > Send Diagostics");
  /* Only selftest supported */
  if(!(target_cmdbuf[1] & 0x4)) {
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

// Unknown command from Macs, disables Unit Attention known to be a problem on Macs
int target_CommandMacUnknown() {
  unit_attention &= ~(1<<target_initiator);
  ncr_dmamode = DMA_NONE;

  target_status = STATUS_CHECK;
  target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
  target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
  target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 0
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x00;
  return 0;
}

// Report Luns
int target_CommandReportLuns() {
  DEBUGPRINT(DBG_TRACE, " > Report Luns");
 /* Check for allocation length to be >= 16Byte */
  if (!target_cmdbuf[6] && !target_cmdbuf[7] && !target_cmdbuf[8] && (target_cmdbuf[9] < 16)) {
    /* Illegal => Prepare sense data */
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST;
    target_sense[target_lun].code = INVALID_FIELD_IN_CDB;
    target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE;
    target_sense[target_lun].key_specific[1] = 0x00;
    target_sense[target_lun].key_specific[2] = 0x06;  /* "Error in Byte 6" */
    return 0;
  }
  TARGET_RESPBUF[0] = 0; // List length (8)
  TARGET_RESPBUF[1] = 0;
  TARGET_RESPBUF[2] = 0;
  TARGET_RESPBUF[3] = 8;

  TARGET_RESPBUF[4] = 0;
  TARGET_RESPBUF[5] = 0;
  TARGET_RESPBUF[6] = 0;
  TARGET_RESPBUF[7] = 0;

  TARGET_RESPBUF[8] = 0;
  TARGET_RESPBUF[9] = 0;
  TARGET_RESPBUF[10] = 0;
  TARGET_RESPBUF[11] = 0;
  TARGET_RESPBUF[12] = 0;
  TARGET_RESPBUF[13] = 0;
  TARGET_RESPBUF[14] = 0;
  TARGET_RESPBUF[15] = 0;

  // Send it
  return target_DIN(TARGET_RESPBUF, 16, 1);
}

int target_ProcessCommand() {
  DEBUGPRINT(DBG_LOWLEVEL, "\r\n> Process");
  //print_cdb();

  if(target_lun == 0xff) {
    target_lun = (target_id<<3) | ((target_cmdbuf[1] >> 5) & 0x7);
    DEBUGPRINT(DBG_LOWLEVEL, " > CDB selected LUN %d", target_lun);
  }

//commandProc:
  switch(target_cmdbuf[0]) {
    case CMD_MAC_UNKNOWN:         return target_CommandMacUnknown();
    case CMD_REQUEST_SENSE:       return target_CommandSense();
  }

  if((target_lun) && ((target_lun >= MAXLUNS) || (!lun[target_lun].Enabled))) {
    DEBUGPRINT(DBG_WARN, " > Request for invalid LUN");
    target_status = STATUS_CHECK;
    target_sense[target_lun].key = ILLEGAL_REQUEST;
    return 0;
  }

  lun[target_lun].Active = 1;

  switch(target_cmdbuf[0]) {
    case CMD_INQUIRY:
      switch(lun[target_lun].Type) {
        default:
        case LUN_DISK_GENERIC:
          return target_DiskInquiry();
#ifdef SUPPORT_OPTICAL
        case LUN_OPTICAL_GENERIC:
          return target_OpticalInquiry();
#endif
#ifdef SUPPORT_ETHERNET
        case LUN_ETHERNET_CABLETRON:
          return target_CabletronInquiry();
#endif
#ifdef SUPPORT_ETHERNET_SCSILINK
        case LUN_ETHERNET_SCSILINK:
          return target_SCSILinkInquiry();
#endif
      }
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = ILLEGAL_REQUEST;
      return 0;
    case CMD_REPORT_LUNS:
      return target_CommandReportLuns();
  }

#ifdef SUPPORT_UNIT_ATTENTION
  /* Check for UNIT ATTENTION condition */
  if (unit_attention & (1<<target_initiator)) {
    DEBUGPRINT(DBG_TRACE, " > Unit Attention");
    /*
     * If command is INQUIRY, REPORT LUNS or REQUEST SENSE, the
     * UNIT ATTENTION condition is preserved and delivered after the next
     * command (as defined by the SAM document)
     */
      
    if (unit_attention_queue[target_initiator] & UA_POTR) {
      DEBUGPRINT(DBG_TRACE, " RESET");
      /* Event: 'Power on or target reset' */
      /* Remove current event from queue */
      unit_attention_queue[target_initiator] &= ~UA_POTR;
      /* Prepare sense data */
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = UNIT_ATTENTION;
      target_sense[target_lun].code = UNIT_POWERON_RESET;
      target_sense[target_lun].qualifier = 0x00;
      target_sense[target_lun].info[0] = 0x00;
      target_sense[target_lun].info[1] = 0x00;
      target_sense[target_lun].info[2] = 0x00;
      target_sense[target_lun].info[3] = 0x00;
      target_sense[target_lun].key_specific[0] = 0x00;
      target_sense[target_lun].key_specific[1] = 0x00;
      target_sense[target_lun].key_specific[2] = 0x00;
    } else if (unit_attention_queue[target_initiator] & UA_NRTRC) {
      DEBUGPRINT(DBG_TRACE, " READY");
      /* Event: 'Not ready to ready change' */
      /* Remove current event from queue */
      unit_attention_queue[target_initiator] &= ~UA_NRTRC;
      /* Prepare sense data */
      target_status = STATUS_CHECK;
      target_sense[target_lun].key = UNIT_ATTENTION;
      target_sense[target_lun].code = NOTREADY_TO_READY_CHANGE;
      target_sense[target_lun].qualifier = 0x00;
      target_sense[target_lun].info[0] = 0x00;
      target_sense[target_lun].info[1] = 0x00;
      target_sense[target_lun].info[2] = 0x00;
      target_sense[target_lun].info[3] = 0x00;
      target_sense[target_lun].key_specific[0] = 0x00;
      target_sense[target_lun].key_specific[1] = 0x00;
      target_sense[target_lun].key_specific[2] = 0x00;
    }
    /* Check if queue is empty now */
    if (!unit_attention_queue[target_initiator]) {
      /* Clear UNIT ATTENTION condition for current initiator */
      unit_attention &= ~(1 << target_initiator);
    }
    if(target_status != STATUS_GOOD)
      return 0;
  }
#endif

  target_status = STATUS_GOOD;
  target_sense[target_lun].key = 0x00; // No Sense Data
  target_sense[target_lun].code = 0x00;
  target_sense[target_lun].qualifier = 0x00;
  target_sense[target_lun].info[0] = 0x00;
  target_sense[target_lun].info[1] = 0x00;
  target_sense[target_lun].info[2] = 0x00;
  target_sense[target_lun].info[3] = 0x00;
  target_sense[target_lun].key_specific[0] = 0x00;
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x00;

#ifdef SUPPORT_RESERVE_RELEASE
  switch(target_cmdbuf[0]) {
    case CMD_RESERVE:             return target_CommandReserveRelease();
    case CMD_RELEASE:             return target_CommandReserveRelease();
  }

  if((target_reservation>=0) && (target_reservation != target_initiator)) {
    target_status = STATUS_CONFLICT;
  }
#endif

  switch(lun[target_lun].Type) {
    case LUN_DISK_GENERIC: {
      int rv = target_DiskProcess();
      if(rv >= 0) return rv;
      break;
    }
#ifdef SUPPORT_OPTICAL
    case LUN_OPTICAL_GENERIC: {
      int rv = target_OpticalProcess();
      if(rv >= 0) return rv;
      break;
    }
#endif
#ifdef SUPPORT_ETHERNET_CABLETRON
    case LUN_ETHERNET_CABLETRON: {
      int rv = target_CabletronProcess();
      if(rv >= 0) return rv;
      break;
    }
#endif
#ifdef SUPPORT_ETHERNET_SCSILINK
    case LUN_ETHERNET_SCSILINK: {
      int rv = target_SCSILinkProcess();
      if(rv >= 0) return rv;
      break;
    }
#endif
  }

//unknownCommand:
  DEBUGPRINT(DBG_GENERAL, " >Unknown Command");
  print_cdb();
  target_status = STATUS_CHECK;
  target_sense[target_lun].key = ILLEGAL_REQUEST; // Illegal Request
  target_sense[target_lun].code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
  target_sense[target_lun].key_specific[0] = ERROR_IN_OPCODE; // Error in opcode
  target_sense[target_lun].key_specific[1] = 0x00;
  target_sense[target_lun].key_specific[2] = 0x00;

  return 0;
}

// Wait to be Selected
int target_SelectInterruptSetup() {
  DEBUGPRINT(DBG_TRACE, " > Select");

  ncr_WriteRegister(SELECT_ENABLE_REG, target_idmask);
  ncr_ReadRegister(RESET_PARITY_INTERRUPT_REG);

  target_phase = PHASE_SELECT;

  return 0;
}

int target_SelectInterruptHandle() {
  uint8_t activeDevices = 0;
  int activeBits = 0;
  int bitCount = 0;
  uint8_t tmp;

  target_phase = PHASE_INVALID;

  // Check for SEL, If not than a BUS Reset has likely occurred,
  // So we need to start over.
  if((ncr_ReadRegister(STATUS_REG) & SR_SEL) != SR_SEL) {
    DEBUGPRINT(DBG_TRACE, " > Reset?");
    return 1;
  }

  //PhaseDelay();

  // Ensure no more than 2 devices are active
  activeDevices = ncr_ReadRegister(CURRENT_SCSI_DATA_REG);
  for(bitCount = 0; bitCount < 8; bitCount++) {
    uint8_t maskbit = (1<<bitCount);
    if(activeDevices & maskbit) {
      if(maskbit & ~target_idmask) target_initiator = bitCount;
      if(maskbit & target_idmask) target_id = bitCount;
      activeBits++;
    }
  }
  if(activeBits != 2) return 1;
  DEBUGPRINT(DBG_TRACE, " > I:%1x/T:%1x", target_initiator, target_id);

  //PhaseDelay();

  // Check we are still selected
  activeDevices = ncr_ReadRegister(CURRENT_SCSI_DATA_REG);
  if(!(activeDevices & target_idmask)) return 1;

  // Assert Busy
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp |= ICR_ASSERT_BSY;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  PhaseDelay();

  // Wait for initiator to Deassert SEL
  if(ncr_Poll(STATUS_REG, SR_SEL, 0, 1000)) return 1;

  // Clear the Select Enable Register and Interrupts
  ncr_WriteRegister(SELECT_ENABLE_REG, 0);
  ncr_ReadRegister(RESET_PARITY_INTERRUPT_REG);

  return 0;
}

// Set Target Mode
int target_Initialize() {
  DEBUGPRINT(DBG_TRACE, "\r\n> Bus Free");

  ncr_WriteRegister(MODE_REG, MR_TARGET);

  // Clear Signals
  ncr_WriteRegister(INITIATOR_COMMAND_REG, 0);
  ncr_WriteRegister(TARGET_COMMAND_REG, 0);
  ncr_WriteRegister(OUTPUT_DATA_REG, 0);
  ncr_WriteRegister(SELECT_ENABLE_REG, 0);

  return 0;
}

void target_Process() {
  int ii;
  DEBUGPRINT(DBG_GENERAL, "\r\n> Target Startup\r\n");

  if(!target_initialized) {
    target_initialized = 1;
    for(ii=0; ii<MAXLUNS; ii++)
      target_sense[ii].key = 0;
  
    /* Queue 'Not ready to ready change, medium may have changed'
     * UNIT ATTENTION condition for all initiators */
    unit_attention |= ~0;
    for (ii = 0; ii < 8; ii++) {
      unit_attention_queue[ii] |= UA_NRTRC;
    }
  }
  
  if(target_interrupt == 1) {
    target_interrupt = 2;
    target_status = STATUS_GOOD;
    target_lun = 0xff;

    attachInterrupt (34, target_ISR, RISING);

    if(target_Initialize()) return;         // Goto BUS FREE state
    if(target_SelectInterruptSetup()) return;    // Wait for SELECTED state
  } else if(target_interrupt == 2) {
    DEBUGPRINT(DBG_GENERAL, "\r\n> Target already running.\r\n");
  } else if(target_interrupt == 0) {
    while(1) {
      target_status = STATUS_GOOD;
      target_lun = 0xff;
    
      if(target_Initialize()) return;         // Goto BUS FREE state
      if(target_SelectInterruptSetup()) return;
    
      // poll for IRQ flag
      while((ncr_ReadRegister(BUS_AND_STATUS_REG) & BASR_IRQ) != BASR_IRQ) if(Serial.available()) return;
    
      target_ISR();
    }
  }
}

void target_ISR() {
  int rv;
  if(target_phase != PHASE_SELECT) return;

//  detachInterrupt (34);  // We are not reentrant, so disable until we are done
  rv = target_SelectInterruptHandle();
  if(rv == 0) {
    if(target_MessageProcess()) goto targetInit;        // Check/Process Host Messages
    if(target_GetCommand()) goto targetInit;            // Get the command
    if(target_cmdlen) {
      if(target_MessageProcess()) goto targetInit;      // Check/Process Host Messages
      if(target_ProcessCommand()) goto targetInit;      // Process Command
    }
    if(target_MessageProcess()) goto targetInit;        // Check/Process Host Messages
    if(target_PutStatus()) goto targetInit;             // Send status
    if(target_MessageProcess()) goto targetInit;        // Check/Process Host Messages

    // Send Command Complete
    target_msg = 0x00;
    if(target_PutMessageIn()) goto targetInit;
    if(target_MessageProcess()) goto targetInit;       // Check/Process Host Messages
    target_status = STATUS_GOOD;
    target_lun = 0xff;
  }

targetInit:
  // Goto BUS FREE state
  if(target_Initialize()) {
    DEBUGPRINT(DBG_GENERAL, "\r\nTarget Handler Abort in Initialize!\r\n");
    target_interrupt = 1;
    return;
  }
  // Prepare for SELECTED state
  if(target_SelectInterruptSetup()) {
    DEBUGPRINT(DBG_GENERAL, "\r\nTarget Handler Abort in Selection!\r\n");
    target_interrupt = 1;
    return;
  }
//  attachInterrupt (34, target_ISR, RISING);
}


void target_SetID(int ID) {
  ID &= 7;
  target_idmask |= (1<<ID);
  if(target_phase == PHASE_SELECT) {
    ncr_WriteRegister(SELECT_ENABLE_REG, target_idmask);
    ncr_ReadRegister(RESET_PARITY_INTERRUPT_REG);
  }
}

void target_ClearID(int ID) {
  ID &= 7;
  target_idmask &= ~(1<<ID);
  if(target_phase == PHASE_SELECT) {
    ncr_WriteRegister(SELECT_ENABLE_REG, target_idmask);
    ncr_ReadRegister(RESET_PARITY_INTERRUPT_REG);
  }
}

int target_GetID() {
  return target_idmask;
}

