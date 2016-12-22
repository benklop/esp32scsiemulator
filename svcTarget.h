#ifndef SVC_TARGET_H
#define SVC_TARGET_H
#include "config.h"
#include "LUNS.h"

typedef struct SenseData_s {
  uint8_t key;
  uint8_t code;
  uint8_t qualifier;
  uint8_t info[4];
  uint8_t key_specific[3];
} SenseData_t;

extern SenseData_t target_sense[MAXLUNS];

#endif /* SVC_TARGET_H */
