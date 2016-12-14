#include "NCR5380.h"
#include "DebugConsole.h"

#define PIN_nRESET (16)
#define PIN_nEOP   (17)
#define PIN_nDACK  (18)
#define PIN_READY  (19)
#define PIN_IRQ    (34)
#define PIN_DRQ    (33)

#define NCR_IO  ((volatile uint8_t *)0xA0000000)
#define NCR_DMA ((volatile uint8_t *)0xB0000000)

#define PARITY

#ifdef DIFFERENTIAL
#define ICR_BASE    ICR_DIFF_ENABLE
#else
#define ICR_BASE    0
#endif

#ifdef PARITY
#define MR_BASE     MR_ENABLE_PAR_CHECK
#else
#define MR_BASE     0
#endif

#if 0
#define CSR_BASE CSR_SCSI_BUFF_INTR | CSR_53C80_INTR
#else
#define CSR_BASE CSR_53C80_INTR
#endif


static struct {
  uint8_t mask;
  const char *name;
} signals[] = {
  {SR_DBP, "PARITY"},
  {SR_RST, "RST"},
  {SR_BSY, "BSY"},
  {SR_REQ, "REQ"},
  {SR_MSG, "MSG"},
  {SR_CD, "CD"},
  {SR_IO, "IO"},
  {SR_SEL, "SEL"},
  {0, NULL}
},
basrs[] = {
  {BASR_END_DMA_TRANSFER, "END OF DMA"},
  {BASR_DRQ, "DRQ"},
  {BASR_PARITY_ERROR, "PARITY ERROR"},
  {BASR_IRQ, "IRQ"},
  {BASR_PHASE_MATCH, "PHASE MATCH"},
  {BASR_BUSY_ERROR, "BUSY ERROR"},
  {BASR_ATN, "ATN"},
  {BASR_ACK, "ACK"},
  {0, NULL}
},
icrs[] = {
  {ICR_ASSERT_RST, "ASSERT RST"},
  {ICR_ARBITRATION_PROGRESS, "ARB. IN PROGRESS"},
  {ICR_ARBITRATION_LOST, "LOST ARB."},
  {ICR_ASSERT_ACK, "ASSERT ACK"},
  {ICR_ASSERT_BSY, "ASSERT BSY"},
  {ICR_ASSERT_SEL, "ASSERT SEL"},
  {ICR_ASSERT_ATN, "ASSERT ATN"},
  {ICR_ASSERT_DATA, "ASSERT DATA"},
  {0, NULL}
},
tcrs[] = {
  {TCR_LAST_BYTE_SENT, "LAST BYTE SENT"},
  {TCR_ASSERT_REQ, "ASSERT REQ"},
  {TCR_ASSERT_MSG, "ASSERT MSG"},
  {TCR_ASSERT_CD, "ASSERT C/D"},
  {TCR_ASSERT_IO, "ASSERT IO"},
  {0, NULL}
},
mrs[] = {
  {MR_BLOCK_DMA_MODE, "BLOCK DMA MODE"},
  {MR_TARGET, "TARGET"},
  {MR_ENABLE_PAR_CHECK, "PARITY CHECK"},
  {MR_ENABLE_PAR_INTR, "PARITY INTR"},
  {MR_ENABLE_EOP_INTR, "EOP INTR"},
  {MR_MONITOR_BSY, "MONITOR BSY"},
  {MR_DMA_MODE, "DMA MODE"},
  {MR_ARBITRATE, "ARBITRATE"},
  {0, NULL}
};

static struct {
        unsigned char value;
        const char *name;
} phases[] = {
        {PHASE_DATAOUT, "DATAOUT"},
        {PHASE_DATAIN, "DATAIN"},
        {PHASE_CMDOUT, "CMDOUT"},
        {PHASE_STATIN, "STATIN"},
        {PHASE_MSGOUT, "MSGOUT"},
        {PHASE_MSGIN, "MSGIN"},
        {PHASE_UNKNOWN, "UNKNOWN"}
};

inline void ncr_Reset(int val) {
  digitalWrite(PIN_nRESET, val);
}

void ncr_Init() {
  pinMode(PIN_nRESET, OUTPUT); // /RESET
  digitalWrite(PIN_nRESET, HIGH);
  pinMode(PIN_nEOP, OUTPUT); // /EOP
  digitalWrite(PIN_nEOP, HIGH);
  pinMode(PIN_DRQ, INPUT); // DRQ
  pinMode(PIN_IRQ, INPUT); // IRQ
  pinMode(PIN_READY, INPUT); // READY

  ncr_Reset(LOW);

  // Set the GPIO ports clocks
  SIM_SCGC5 = SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTC | SIM_SCGC5_PORTD | SIM_SCGC5_PORTE;

  // FlexBus Controller
  SIM_SCGC7 |= SIM_SCGC7_FLEXBUS; // Enable FlexBus Clock
  SIM_CLKDIV1 = (SIM_CLKDIV1 & ~SIM_CLKDIV1_OUTDIV3(0xf)) | SIM_CLKDIV1_OUTDIV3(3); // 12.5MHz Clock

  PORTD_PCR6  = PORT_PCR_MUX(5); // D0
  PORTD_PCR5  = PORT_PCR_MUX(5); // D1
  PORTD_PCR4  = PORT_PCR_MUX(5); // D2
  PORTD_PCR3  = PORT_PCR_MUX(5); // D3
  PORTD_PCR2  = PORT_PCR_MUX(5); // D4
  PORTC_PCR10 = PORT_PCR_MUX(5); // D5
  PORTC_PCR9  = PORT_PCR_MUX(5); // D6
  PORTC_PCR8  = PORT_PCR_MUX(5); // D7

  PORTD_PCR0 = PORT_PCR_MUX(5); // /DACK
  PORTD_PCR1 = PORT_PCR_MUX(5); // /CS

  PORTC_PCR7 = PORT_PCR_MUX(5); // A0 (AD8)
  PORTC_PCR6 = PORT_PCR_MUX(5); // A1 (AD9)
  PORTC_PCR5 = PORT_PCR_MUX(5); // A2 (AD10)

  PORTC_PCR11 = PORT_PCR_MUX(5); // /IOW (R/W)
  PORTB_PCR19 = PORT_PCR_MUX(5); // /IOR (/OE)

  FB_CSAR0 = 0xA0000000;
  FB_CSCR0 = FB_CSCR_BLS | FB_CSCR_PS(1) | FB_CSCR_AA | FB_CSCR_ASET(1) | FB_CSCR_WS(3);
  FB_CSMR0 = FB_CSMR_BAM(0) | FB_CSMR_V;
  FB_CSAR1 = 0xB0000000;
  FB_CSCR1 = FB_CSCR_BLS | FB_CSCR_PS(1) | FB_CSCR_AA | FB_CSCR_ASET(1) | FB_CSCR_WS(3);
  FB_CSMR1 = FB_CSMR_BAM(0) | FB_CSMR_V;
  FB_CSPMCR = (1<<28) | (2<<12);
  delayMicroseconds(10);

  ncr_Reset(HIGH);

  delayMicroseconds(10);

  ncr_WriteRegister(INITIATOR_COMMAND_REG, ICR_BASE);
  ncr_WriteRegister(MODE_REG, MR_BASE);
  ncr_WriteRegister(TARGET_COMMAND_REG, 0);
  ncr_WriteRegister(SELECT_ENABLE_REG, 0);
}

void ncr_BusReset() {
  ncr_WriteRegister(MODE_REG, MR_BASE);
  ncr_WriteRegister(TARGET_COMMAND_REG, 0);
  ncr_WriteRegister(SELECT_ENABLE_REG, 0);
}

uint8_t ncr_ReadRegister(int reg) {
  uint8_t dat = NCR_IO[(reg & 7) << 8];
//  DEBUGPRINT(DBG_HOLYCRAP, "\n\r\t) READ %x: %2x", reg, dat);
  return dat;
}

void ncr_WriteRegister(int reg, uint8_t dat) {
//  DEBUGPRINT(DBG_HOLYCRAP, "\n\r\t) WRITE %x: %2x", reg, dat);
  NCR_IO[((reg & 7) << 8) | dat] = dat;
}


inline uint8_t ncr_ReadDMA() {
  return *NCR_DMA;
}

inline void ncr_WriteDMA(uint8_t dat) {
  NCR_DMA[dat] = dat;
}

int ncr_Poll(int reg, uint8_t bMask, uint8_t bVal, uint32_t timeout) {
  elapsedMillis pollTimeout;
  do {
    if((ncr_ReadRegister(reg) & bMask) == bVal) return 0;
  } while (pollTimeout < timeout);
  return pollTimeout;
}

int ncr_Poll2(int regA, uint8_t bMaskA, uint8_t bValA, int regB, uint8_t bMaskB, uint8_t bValB, uint32_t timeout) {
  elapsedMillis pollTimeout;
  do {
    if((ncr_ReadRegister(regA) & bMaskA) == bValA) return 0;
    if((ncr_ReadRegister(regB) & bMaskB) == bValB) return 0;
  } while (pollTimeout < timeout);
  return pollTimeout;
}

void ncr_Print() {
  uint8_t status, data, basr, mr, icr, tcr, i;
 
  data = ncr_ReadRegister(CURRENT_SCSI_DATA_REG);
  status = ncr_ReadRegister(STATUS_REG);
  mr = ncr_ReadRegister(MODE_REG);
  icr = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tcr = ncr_ReadRegister(TARGET_COMMAND_REG);
  basr = ncr_ReadRegister(BUS_AND_STATUS_REG);

  DEBUGPRINT(DBG_GENERAL, "DR =   0x%02x\n", data);
  DEBUGPRINT(DBG_GENERAL, "SR =   0x%02x : ", status);
  for (i = 0; signals[i].mask; ++i)
    if (status & signals[i].mask)
      DEBUGPRINT(DBG_GENERAL, "%s, ", signals[i].name);
  DEBUGPRINT(DBG_GENERAL, "\nBASR = 0x%02x : ", basr);
  for (i = 0; basrs[i].mask; ++i)
    if (basr & basrs[i].mask)
      DEBUGPRINT(DBG_GENERAL, "%s, ", basrs[i].name);
  DEBUGPRINT(DBG_GENERAL, "\nICR =  0x%02x : ", icr);
  for (i = 0; icrs[i].mask; ++i)
    if (icr & icrs[i].mask)
      DEBUGPRINT(DBG_GENERAL, "%s, ", icrs[i].name);
  DEBUGPRINT(DBG_GENERAL, "\nTCR =  0x%02x : ", tcr);
  for (i = 0; tcrs[i].mask; ++i)
    if (tcr & tcrs[i].mask)
      DEBUGPRINT(DBG_GENERAL, "%s, ", tcrs[i].name);
  DEBUGPRINT(DBG_GENERAL, "\nMR =   0x%02x : ", mr);
  for (i = 0; mrs[i].mask; ++i)
    if (mr & mrs[i].mask)
      DEBUGPRINT(DBG_GENERAL, "%s, ", mrs[i].name);
  DEBUGPRINT(DBG_GENERAL, "\r\n");
}

/**
 * NCR5380_print_phase - show SCSI phase
 * @instance: adapter to dump
 *
 * Print the current SCSI phase for debugging purposes
 */

void NCR5380_print_phase() {
  unsigned char status;
  int i;

  status = ncr_ReadRegister(STATUS_REG);
  if (!(status & SR_REQ)) {
    DEBUGPRINT(DBG_GENERAL, "REQ not asserted, phase unknown.\n");
  } else {
    for (i = 0; (phases[i].value != PHASE_UNKNOWN) && (phases[i].value != (status & PHASE_MASK)); ++i)
      ;
    DEBUGPRINT(DBG_GENERAL, "phase %s\n", phases[i].name);
  }
}

int ncr_Put(uint8_t *buf, uint16_t len, uint16_t count) {
  if(len == 1)
    return ncr_PutPIO(buf, len, count);
  switch(ncr_dmamode) {
    case DMA_REAL:
      return ncr_PutRDMA(buf, len, count);
    case DMA_PSEUDO:
      return ncr_PutPDMA(buf, len, count);
    default:
      return ncr_PutPIO(buf, len, count);
  }
}

int ncr_Get(uint8_t *buf, uint16_t len, uint16_t count) {
  if(len == 1)
    return ncr_GetPIO(buf, len, count);
  switch(ncr_dmamode) {
    case DMA_REAL:
      return ncr_GetRDMA(buf, len, count);
    case DMA_PSEUDO:
      return ncr_GetPDMA(buf, len, count);
    default:
      return ncr_GetPIO(buf, len, count);
  }
}

// Transfer bytes from controller to memory REAL_DMA
int ncr_GetRDMA(uint8_t *buf, uint16_t len, uint16_t count) {
  uint16_t x, y;
  uint8_t tmp;
  DEBUGPRINT(DBG_DMA, "\n\r\t>Get (Real DMA) %d %d", len, count);

  // Deassert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp &= ~ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  // Set DMA mode
  tmp = ncr_ReadRegister(MODE_REG);
  tmp |= MR_DMA_MODE;
  ncr_WriteRegister(MODE_REG, tmp);
  ncr_WriteRegister(START_DMA_TARGET_RECEIVE_REG, tmp);

  y = count;
  while(y--) {
    x = len;
    while(x--) {
      // Wait for DRQ
      while(!digitalRead(PIN_DRQ));

      // Check for last byte
      if((y == 0) && (x == 0)) {
        // Clear DMA mode
        tmp = ncr_ReadRegister(MODE_REG);
        tmp &= ~MR_DMA_MODE;
        ncr_WriteRegister(MODE_REG, tmp);
        digitalWrite(PIN_nEOP, LOW);
      }

      *(buf++) = ncr_ReadDMA(); // nDACK
    };
  };

  digitalWrite(PIN_nEOP, HIGH);

  return 0;
}

// Transfer bytes from memory to controller PSEUDO_DMA
int ncr_PutRDMA(uint8_t *buf, uint16_t len, uint16_t count) {
  uint16_t y, x;
  uint8_t tmp;

  DEBUGPRINT(DBG_DMA, "\n\r\t>Put (Real DMA) %d %d", len, count);

  // Assert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp |= ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);
  
  // Set DMA mode
  tmp = ncr_ReadRegister(MODE_REG);
  tmp |= MR_DMA_MODE;
  ncr_WriteRegister(MODE_REG, tmp);
  ncr_WriteRegister(START_DMA_SEND_REG, tmp);

  y = count;
  while(y--) {
    x = len;
    while(x--) {
      // Wait for DRQ
      while(!digitalRead(PIN_DRQ));

      // Check for last byte
      //if((y == 0) && (x == 0)) {
      //  digitalWrite(PIN_nEOP, LOW);
      //}

      ncr_WriteDMA(*(buf++));
    };
  };

  if(ncr_Poll(BUS_AND_STATUS_REG, BASR_DRQ, BASR_DRQ, 1000)) return 1;

  //digitalWrite(PIN_nEOP, HIGH);
  // Clear DMA mode
  tmp = ncr_ReadRegister(MODE_REG);
  tmp &= ~MR_DMA_MODE;
  ncr_WriteRegister(MODE_REG, tmp);

  // Deassert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp &= ~ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  return 0;
}

// Transfer bytes from controller to memory PSEUDO_DMA
int ncr_GetPDMA(uint8_t *buf, uint16_t len, uint16_t count) {
  uint16_t x, y;
  uint8_t tmp;
  DEBUGPRINT(DBG_DMA, "\n\r\t>Get (Pseudo DMA) %d %d", len, count);

  // Deassert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp &= ~ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  // Set DMA mode
  tmp = ncr_ReadRegister(MODE_REG);
  tmp |= MR_DMA_MODE;
  ncr_WriteRegister(MODE_REG, tmp);
  ncr_WriteRegister(START_DMA_TARGET_RECEIVE_REG, tmp);

  y = count;
  while(y--) {
    x = len;
    while(x--) {
      // Wait for DRQ
      if(ncr_Poll(BUS_AND_STATUS_REG, BASR_DRQ, BASR_DRQ, 1000)) return 1;

      // Check for last byte
      if((y == 0) && (x == 0)) {
        // Clear DMA mode
        tmp = ncr_ReadRegister(MODE_REG);
        tmp &= ~MR_DMA_MODE;
        ncr_WriteRegister(MODE_REG, tmp);
      }

      *(buf++) = ncr_ReadDMA(); // nDACK
    };
  };
  
  return 0;
}

// Transfer bytes from memory to controller PSEUDO_DMA
int ncr_PutPDMA(uint8_t *buf, uint16_t len, uint16_t count) {
  uint16_t x, y;
  uint8_t tmp;
  DEBUGPRINT(DBG_DMA, "\n\r\t>Put (Pseudo DMA) %d %d", len, count);

  // Assert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp |= ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);
  
  // Set DMA mode
  tmp = ncr_ReadRegister(MODE_REG);
  tmp |= MR_DMA_MODE;
  ncr_WriteRegister(MODE_REG, tmp);
  ncr_WriteRegister(START_DMA_SEND_REG, tmp);

  y = count;
  while(y--) {
    x = len;
    while(x--) {
      // Wait for DRQ
      if(ncr_Poll(BUS_AND_STATUS_REG, BASR_DRQ, BASR_DRQ, 1000)) return 1;

      ncr_WriteDMA(*(buf++));
    };
  };

  if(ncr_Poll(BUS_AND_STATUS_REG, BASR_DRQ, BASR_DRQ, 1000)) return 1;

  // Clear DMA mode
  tmp = ncr_ReadRegister(MODE_REG);
  tmp &= ~MR_DMA_MODE;
  ncr_WriteRegister(MODE_REG, tmp);

  // Deassert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp &= ~ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  return 0;
}

// Transfer bytes from controller to memory POLLED_IO
int ncr_GetPIO(uint8_t *buf, uint16_t len, uint16_t count) {
  uint8_t tmp;
  uint16_t x, y;

  DEBUGPRINT(DBG_PIO, "\n\r\t>Get (PIO) %d %d", len, count);
  
  // Deassert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp &= ~ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  y = count;
  while(y--) {
    x = len;
    while(x--) {
      // Assert REQ
      tmp = ncr_ReadRegister(TARGET_COMMAND_REG);
      tmp |= TCR_ASSERT_REQ;
      ncr_WriteRegister(TARGET_COMMAND_REG, tmp);
  
      // Wait for ACK
      if(ncr_Poll(BUS_AND_STATUS_REG, BASR_ACK, BASR_ACK, 1000)) return 0;
  
      *(buf++) = ncr_ReadRegister(CURRENT_SCSI_DATA_REG);

      // Deassert REQ
      tmp = ncr_ReadRegister(TARGET_COMMAND_REG);
      tmp &= ~TCR_ASSERT_REQ;
      ncr_WriteRegister(TARGET_COMMAND_REG, tmp);

      // Wait for ACK to Deassert
      if(ncr_Poll(BUS_AND_STATUS_REG, BASR_ACK, 0, 1000)) return 0;
    }
  }

  return 0;
}

// Transfer bytes from memory to controller POLLED_IO
int ncr_PutPIO(uint8_t *buf, uint16_t len, uint16_t count) {
  uint8_t tmp;
  uint16_t x, y;
  
  DEBUGPRINT(DBG_PIO, "\n\r\t>Put (PIO) %d %d", len, count);
  
  // Assert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp |= ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  y = count;
  while(y--) {
    x = len;
    while(x--) {
      ncr_WriteRegister(OUTPUT_DATA_REG, *(buf++));

      // Assert REQ
      tmp = ncr_ReadRegister(TARGET_COMMAND_REG);
      tmp |= TCR_ASSERT_REQ;
      ncr_WriteRegister(TARGET_COMMAND_REG, tmp);
  
      // Wait for ACK
      if(ncr_Poll(BUS_AND_STATUS_REG, BASR_ACK, BASR_ACK, 1000)) return 0;

      // Deassert REQ
      tmp = ncr_ReadRegister(TARGET_COMMAND_REG);
      tmp &= ~TCR_ASSERT_REQ;
      ncr_WriteRegister(TARGET_COMMAND_REG, tmp);

      // Wait for ACK to Deassert
      if(ncr_Poll(BUS_AND_STATUS_REG, BASR_ACK, 0, 1000)) return 0;
    }
  }

  // Deassert data bus
  tmp = ncr_ReadRegister(INITIATOR_COMMAND_REG);
  tmp &= ~ICR_ASSERT_DATA;
  ncr_WriteRegister(INITIATOR_COMMAND_REG, tmp);

  return 0;
}


