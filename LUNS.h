#ifndef LUNS_H
#define LUNS_H

#define MAXLUNS 64
typedef struct LUN_s {
  uint8_t Enabled;
  uint8_t Type;
  uint8_t WriteProtect;
  uint8_t Mounted;
  uint8_t Vendor[8];
  uint8_t Model[16];
  uint8_t Revision[4];
  uint8_t Unique[8];
  uint8_t HWAddr[6];
  uint8_t MediaType;
  uint8_t ReportAutoCorrect;
  uint8_t Quirks;
  volatile int Active;
  unsigned long Offset;
  unsigned long Size;
  unsigned long Cylinders;
  uint8_t Heads;
  uint16_t Sectors;
  uint16_t SectorSize;
} LUN_t;
extern LUN_t lun[MAXLUNS];
extern volatile uint8_t luns;

#define LUN_DISK_GENERIC       0x11
#define LUN_OPTICAL_GENERIC    0x21
#define LUN_TAPE_GENERIC       0x31
#define LUN_ETHERNET_CABLETRON 0x41
#define LUN_ETHERNET_SCSILINK  0x42

/* Initiator specific quirks and work arounds */
#define QUIRKS_APPLE 0x01

#endif
