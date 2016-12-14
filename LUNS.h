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
  volatile int Active;
  unsigned long Offset;
  unsigned long Size;
} LUN_t;
extern LUN_t lun[MAXLUNS];
extern volatile uint8_t luns;

#define LUN_DISK     0x1
#define LUN_OPTICAL  0x2
#define LUN_ETHERNET 0x3
#define LUN_TAPE     0x4

#endif
