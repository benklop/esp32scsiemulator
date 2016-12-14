#ifndef PARSEMBR_H
#define PARSEMBR_H

typedef struct PART_s {
  unsigned long Offset;
  unsigned long Size;
  unsigned char Type;
} PART_t;

#define MAXTABLES 64
typedef struct TABLE_s {
  unsigned long Offset;
  unsigned long Extended;
  PART_t Part[4];
} TABLE_t;
extern TABLE_t pTable[MAXTABLES];
extern volatile int pTables;

#endif
