#ifndef SVC_ETHERNET_H
#define SVC_ETHERNET_H
#include "config.h"
#if defined(SUPPORT_ETHERNET_SCSILINK) || defined(SUPPORT_ETHERNET_CABLETRON)


#define NETBUFSIZE 0x4000
#define NETBUFMASK 0x3FFF

typedef struct cb_packet_s {
  uint8_t Buffer[NETBUFSIZE];
  uint16_t Head;
  uint16_t Tail;
} cb_packet_t;

extern cb_packet_t cPacketOut;
extern cb_packet_t cPacketIn;

int cbpeek(cb_packet_t *buf);
uint16_t cbpush(cb_packet_t *buf, uint8_t *pkt, uint16_t sze);
uint16_t cbpop(cb_packet_t *buf, uint8_t *pkt, uint16_t msz);

void ethernetInit();
void ethernetSetMAC(uint8_t *newmac);
void ethernetService();
uint16_t target_EthernetSend(uint8_t *pkt, uint16_t sze);
uint16_t target_EthernetRecv(uint8_t *pkt, uint16_t sze);

#endif /* SUPPORT_ETHERNET_SCSILINK || SUPPORT_ETHERNET_CABLETRON */
#endif /* SVC_ETHERNET_H */
