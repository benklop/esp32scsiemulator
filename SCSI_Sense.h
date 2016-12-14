#ifndef SCSI_SENSE_H
#define SCSI_SENSE_H

/* Task states */
#define ENDED            0x00
#define CURRENT          0x01

/* Status */
#define STATUS_GOOD           0x00
#define STATUS_CHECK          0x02
#define STATUS_BUSY           0x08
#define STATUS_INTERMEDIATE   0x10
#define STATUS_CONFLICT       0x18

/* Sense keys */
#define NO_SENSE         0x00
#define RECOVERED_ERROR  0x01
#define NOT_READY        0x02
#define MEDIUM_ERROR     0x03
#define HARDWARE_ERROR   0x04
#define ILLEGAL_REQUEST  0x05
#define UNIT_ATTENTION   0x06
#define DATA_PROTECT     0x07

/* Additional Sense Information */
#define NO_ADDITIONAL_SENSE_INFORMATION 0x00
#define INVALID_LBA 0x21
#define INVALID_FIELD_IN_CDB 0x24
#define NOTREADY_TO_READY_CHANGE 0x28
#define UNIT_POWERON_RESET   0x29

#define ERROR_IN_OPCODE      0xC0


#endif

