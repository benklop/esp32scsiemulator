// Attempt target selection
typedef struct InitiatorCmd_s {
  uint8_t target;
  uint8_t lun;
  uint8_t cmdbuf[16];
  uint8_t result[16];
} InitiatorCmd_t;

int initiator_Select(void *cmd);
