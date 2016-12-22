#ifndef NCR5380_H
#define NCR5380_H

extern int ncr_dmamode;

#define DMA_NONE   0
#define DMA_PSEUDO 1
#define DMA_REAL   2
#define DMA_BLOCK  3

extern int phasedelay;

#define PHASEDELAY phasedelay
#ifndef PHASEDELAY
#define PhaseDelay() {}
#else
#define PhaseDelay() delayMicroseconds(phasedelay)
#endif

/* 
 * The contents of the OUTPUT DATA register are asserted on the bus when
 * either arbitration is occurring or the phase-indicating signals (
 * IO, CD, MSG) in the TARGET COMMAND register and the ASSERT DATA
 * bit in the INITIATOR COMMAND register is set.
 */

#define OUTPUT_DATA_REG         0  /* wo DATA lines on SCSI bus */
#define CURRENT_SCSI_DATA_REG   0 /* ro same */

#define INITIATOR_COMMAND_REG 1 /* rw */
#define ICR_ASSERT_RST    0x80  /* rw Set to assert RST  */
#define ICR_ARBITRATION_PROGRESS 0x40 /* ro Indicates arbitration complete */
#define ICR_TRI_STATE   0x40  /* wo Set to tri-state drivers */
#define ICR_ARBITRATION_LOST  0x20  /* ro Indicates arbitration lost */
#define ICR_DIFF_ENABLE   0x20  /* wo Set to enable diff. drivers */
#define ICR_ASSERT_ACK    0x10  /* rw ini Set to assert ACK */
#define ICR_ASSERT_BSY    0x08  /* rw Set to assert BSY */
#define ICR_ASSERT_SEL    0x04  /* rw Set to assert SEL */
#define ICR_ASSERT_ATN    0x02  /* rw Set to assert ATN */
#define ICR_ASSERT_DATA   0x01  /* rw SCSI_DATA_REG is asserted */

#define MODE_REG    2
/*
 * Note : BLOCK_DMA code will keep DRQ asserted for the duration of the 
 * transfer, causing the chip to hog the bus.  You probably don't want 
 * this.
 */
#define MR_BLOCK_DMA_MODE 0x80  /* rw block mode DMA */
#define MR_TARGET   0x40  /* rw target mode */
#define MR_ENABLE_PAR_CHECK 0x20  /* rw enable parity checking */
#define MR_ENABLE_PAR_INTR  0x10  /* rw enable bad parity interrupt */
#define MR_ENABLE_EOP_INTR  0x08  /* rw enable eop interrupt */
#define MR_MONITOR_BSY    0x04  /* rw enable int on unexpected bsy fail */
#define MR_DMA_MODE   0x02  /* rw DMA / pseudo DMA mode */
#define MR_ARBITRATE    0x01  /* rw start arbitration */

#define TARGET_COMMAND_REG  3
#define TCR_LAST_BYTE_SENT  0x80  /* ro DMA done */
#define TCR_ASSERT_REQ    0x08  /* tgt rw assert REQ */
#define TCR_ASSERT_MSG    0x04  /* tgt rw assert MSG */
#define TCR_ASSERT_CD   0x02  /* tgt rw assert CD */
#define TCR_ASSERT_IO   0x01  /* tgt rw assert IO */

#define STATUS_REG    4 /* ro */
/*
 * Note : a set bit indicates an active signal, driven by us or another 
 * device.
 */
#define SR_RST      0x80
#define SR_BSY      0x40
#define SR_REQ      0x20
#define SR_MSG      0x10
#define SR_CD     0x08
#define SR_IO     0x04
#define SR_SEL      0x02
#define SR_DBP      0x01

/*
 * Setting a bit in this register will cause an interrupt to be generated when 
 * BSY is false and SEL true and this bit is asserted  on the bus.
 */
#define SELECT_ENABLE_REG 4 /* wo */

#define BUS_AND_STATUS_REG  5 /* ro */
#define BASR_END_DMA_TRANSFER 0x80  /* ro set on end of transfer */
#define BASR_DRQ    0x40  /* ro mirror of DRQ pin */
#define BASR_PARITY_ERROR 0x20  /* ro parity error detected */
#define BASR_IRQ    0x10  /* ro mirror of IRQ pin */
#define BASR_PHASE_MATCH  0x08  /* ro Set when MSG CD IO match TCR */
#define BASR_BUSY_ERROR   0x04  /* ro Unexpected change to inactive state */
#define BASR_ATN    0x02  /* ro BUS status */
#define BASR_ACK    0x01  /* ro BUS status */

/* Write any value to this register to start a DMA send */
#define START_DMA_SEND_REG  5 /* wo */

/* 
 * Used in DMA transfer mode, data is latched from the SCSI bus on
 * the falling edge of REQ (ini) or ACK (tgt)
 */
#define INPUT_DATA_REG      6 /* ro */

/* Write any value to this register to start a DMA receive */
#define START_DMA_TARGET_RECEIVE_REG  6 /* wo */

/* Read this register to clear interrupt conditions */
#define RESET_PARITY_INTERRUPT_REG  7 /* ro */

/* Write any value to this register to start an ini mode DMA receive */
#define START_DMA_INITIATOR_RECEIVE_REG 7 /* wo */

#define CSR_53C80_REG          0x80 /* ro  5380 registers busy */
#define CSR_TRANS_DIR          0x40 /* rw  Data transfer direction */
#define CSR_SCSI_BUFF_INTR     0x20 /* rw  Enable int on transfer ready */
#define CSR_53C80_INTR         0x10 /* rw  Enable 53c80 interrupts */
#define CSR_SHARED_INTR        0x08 /* rw  Interrupt sharing */
#define CSR_HOST_BUF_NOT_RDY   0x04 /* ro  Is Host buffer ready */
#define CSR_SCSI_BUF_RDY       0x02 /* ro  SCSI buffer read */
#define CSR_GATED_53C80_IRQ    0x01 /* ro  Last block xferred */

/* Note : PHASE_* macros are based on the values of the STATUS register */
#define PHASE_MASK  (SR_MSG | SR_CD | SR_IO)

#define PHASE_DATAOUT   0
#define PHASE_DATAIN    SR_IO
#define PHASE_CMDOUT    SR_CD
#define PHASE_STATIN    (SR_CD | SR_IO)
#define PHASE_MSGOUT    (SR_MSG | SR_CD)
#define PHASE_MSGIN   (SR_MSG | SR_CD | SR_IO)
#define PHASE_UNKNOWN   0xff

/* 
 * Convert status register phase to something we can use to set phase in 
 * the target register so we can get phase mismatch interrupts on DMA 
 * transfers.
 */

#define PHASE_SR_TO_TCR(phase) ((phase) >> 2)


#endif

