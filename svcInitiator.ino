#include "svcInitiator.h"
#include "scsiMessages.h"
#include "scsiCommands.h"
#include "scsiSense.h"
#include "DebugConsole.h"
#include "drvNCR5380.h"

uint8_t initiator_id;
uint8_t initiator_msg[16];

#define MR_BASE MR_ENABLE_PAR_CHECK
#define ICR_BASE 0

void initiator_Reset() {
  uint8_t tmp = ncr_ReadRegister(STATUS_REG) & PHASE_MASK;
  ncr_WriteRegister(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_RST);
  delayMicroseconds(50);
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
  ncr_ReadRegister(RESET_PARITY_INTERRUPT_REG);
}

int initiator_Abort() {
  int rc;
  uint8_t tmp;
  int32_t len;
  uint8_t phase;
  uint8_t *dataptr;

  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
  rc = ncr_Poll(STATUS_REG, SR_REQ, SR_REQ, 10000);
  if(rc) goto timeout;

  tmp = ncr_ReadRegister(STATUS_REG) & PHASE_MASK;
  ncr_WriteRegister(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));
  if(tmp != PHASE_MSGOUT) {
    ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
    rc = ncr_Poll(STATUS_REG, SR_REQ, 0, 3000);
    if(rc) goto timeout;
    ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
  }

  initiator_msg[0] = ABORT_TASK_SET;
  len = 1;
  phase = PHASE_MSGOUT;
  dataptr = initiator_msg;
  initiator_TransferPIO(&phase, &len, &dataptr);

  return len ? -1 : 0;

timeout:
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
  return -1;
}

void initiator_Reselect() {
}

int initiator_PutMessageOut() {
  int32_t len = 1;
  uint8_t phase = PHASE_MSGOUT;
  uint8_t *dataptr = initiator_msg;

  if(initiator_msg[0] == EXTENDED_MESSAGE) {
    if(initiator_msg[1] == 0) {
      len = 256 + 2;
    } else {
      len = initiator_msg[1] + 2;
    }
  }
  
  initiator_TransferPIO(&phase, &len, &dataptr);

  return len ? -1 : 0;
}

#if 0
int initiator_GetMessageIn() {
  int32_t len = 1;
  uint8_t phase = PHASE_MSGIN;
  uint8_t *dataptr = initiator_msg;

  if(initiator_msg[0] == EXTENDED_MESSAGE) {
    if(initiator_msg[1] == 0)
      len = 256 + 2;
    else
      len = initiator_msg[1] + 2;
  }
  
  initiator_TransferPIO(&phase, &len, &dataptr);

  return len ? -1 : 0;
}
#endif

int initiator_TransferPIO(uint8_t *phase, int32_t *count, uint8_t **data) {
  uint8_t p = *phase;
  int32_t c = *count;
  uint8_t *d = *data;
  uint8_t tmp;

  ncr_WriteRegister(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(p));

  do {
    // Wait for assertion of REQ for phase bits to be valid
    if(ncr_Poll(STATUS_REG, SR_REQ, SR_REQ, 1000)) break;

    DEBUGPRINT(DBG_GENERAL, "\r\n>REQ Asserted");

    // Check for phase mismatch
    if((ncr_ReadRegister(STATUS_REG) & PHASE_MASK) != p) {
      DEBUGPRINT(DBG_GENERAL, "\r\n>Phase Mismatch");
      break;
    }

    if(!(p&SR_IO))
      ncr_WriteRegister(OUTPUT_DATA_REG, *d);
    else
      *d = ncr_ReadRegister(CURRENT_SCSI_DATA_REG);

    d++;

    // Initiator should drop ATEN in MSGOUT phase on last byte
    // after REQ has been asserted but before ACK is raised
    if(!(p&SR_IO)) {
      if(!((p&SR_MSG) && (c > 1))) {
        ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA);
        ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ACK);
      } else {
        ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ATN);
        ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
      }
    } else {
      ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
    }

    if(ncr_Poll(STATUS_REG, SR_REQ, 0, 5000)) break;

    DEBUGPRINT(DBG_GENERAL, "\r\n>REQ Deasserted");

    if(!((p == PHASE_MSGIN) && (c == 1))) {
      if((p == PHASE_MSGOUT) && (c > 1)) {
        ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
      } else {
        ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
      }
    }
  } while (--c);

  *count = c;
  *data = d;

  tmp = ncr_ReadRegister(STATUS_REG);
  if((tmp & SR_REQ) || ((tmp & SR_IO) && (c==0))) {
    *phase = tmp & PHASE_MASK;
  } else {
    *phase = PHASE_UNKNOWN;
  }
  if(!c || (*phase == p)) {
    return 0;
  }

  return -1;
}

int initiator_Select(void *pcmd) {
  InitiatorCmd_t *cmd = (InitiatorCmd_t *)pcmd;
  int err;
  uint8_t higher_ids;
  DEBUGPRINT(DBG_GENERAL, " > Select");

  higher_ids = (0xfe << initiator_id);

  ncr_WriteRegister(TARGET_COMMAND_REG, 0);
  ncr_WriteRegister(OUTPUT_DATA_REG, (1<<initiator_id));
  ncr_WriteRegister(MODE_REG, MR_ARBITRATE);

  // Chip waits for BUS FREE
  err = ncr_Poll2(MODE_REG, MR_ARBITRATE, 0,
    INITIATOR_COMMAND_REG, ICR_ARBITRATION_PROGRESS, ICR_ARBITRATION_PROGRESS, 1000);

  // Reselection Interrupt
  if((ncr_ReadRegister(MODE_REG) & MR_ARBITRATE) != MR_ARBITRATE) return 0;

  if(err) {
    ncr_WriteRegister(MODE_REG, MR_BASE);
    DEBUGPRINT(DBG_GENERAL, "\r\n>Arbitration Timeout");
    return 0;
  }

  // Check for lost arbitration
  if((ncr_ReadRegister(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST) ||
     (ncr_ReadRegister(CURRENT_SCSI_DATA_REG) & higher_ids) ||
     (ncr_ReadRegister(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST)) {
    ncr_WriteRegister(MODE_REG, MR_BASE);
    DEBUGPRINT(DBG_GENERAL, "\r\n>Lost Arbitration");
    return 0;
  }

  // Assert BSY after/during arbitration
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_SEL | ICR_ASSERT_BSY);

  // Let the bus settle
  PhaseDelay();

  // Final check to make sure we won.
  if((ncr_ReadRegister(MODE_REG) & MR_ARBITRATE) != MR_ARBITRATE) return 0;

  DEBUGPRINT(DBG_GENERAL, "\r\n>Won Arbitration");

  // Start selection process by asserting Initiator & Target IDs
  ncr_WriteRegister(OUTPUT_DATA_REG, initiator_id | (1<<(cmd->target)));

  /*
   * Raise ATN while SEL is true before BSY goes false from arbitration,
   * since this is the only way to guarantee that we'll get a MESSAGE OUT
   * phase immediately after selection.
   */
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_BSY | ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_SEL);
  ncr_WriteRegister(MODE_REG, MR_BASE);

  // Disable reselect interrupts so we don't trigger one by accident
  ncr_WriteRegister(SELECT_ENABLE_REG, 0);
  
  // Wait atleast two "deskew" delays
  PhaseDelay();

  // De-assert BSY
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_SEL);
  
  // Wait another "deskew" delay
  PhaseDelay();

  err = ncr_Poll(STATUS_REG, SR_BSY, SR_BSY, 250);
  if((ncr_ReadRegister(STATUS_REG) & (SR_SEL | SR_IO)) == (SR_SEL | SR_IO)) {
    ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
    initiator_Reselect();
    ncr_WriteRegister(SELECT_ENABLE_REG, initiator_id);
    DEBUGPRINT(DBG_GENERAL, "\r\n>Reselection after Won Arbitration?");
    return 0;
  }

  if(err) {
    ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
    ncr_WriteRegister(SELECT_ENABLE_REG, initiator_id);
    cmd->result[0] = NOT_READY;
    DEBUGPRINT(DBG_GENERAL, "\r\n>Target did not reply within 250ms");
    return 0;
  }
  
  // Wait another two "deskew" delays
  PhaseDelay();

  // Release SEL
  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);

  // Wait for start of REQ/ACK
  err = ncr_Poll(STATUS_REG, SR_REQ, SR_REQ, 1000);
  if(err) {
    ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
    ncr_WriteRegister(SELECT_ENABLE_REG, initiator_id);
    cmd->result[0] = NOT_READY;
    DEBUGPRINT(DBG_GENERAL, "\r\n>REQ timeout during selection");
    return 0;
  }

  initiator_msg[0] = IDENTIFY | (cmd->lun & 0x7);
  initiator_PutMessageOut();

  return 0;
}
