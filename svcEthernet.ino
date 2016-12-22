#include "config.h"
#if defined(SUPPORT_ETHERNET_SCSILINK) || defined(SUPPORT_ETHERNET_CABLETRON)
#include <SPI.h>
#include <EtherRaw.h>
#include <utility/w5100.h>
#include "svcEthernet.h"

static uint8_t mac_addr[6];
uint8_t mac_addr_service = 0;


cb_packet_t cPacketOut;
cb_packet_t cPacketIn;

static uint8_t ePacketBuf[10240];

// Does buffer have data?
int cbpeek(cb_packet_t *buf) {
  return (buf->Head != buf->Tail);
}

// Increase Tail position and wrap, returning old position
static inline int cbpi(cb_packet_t *buf) {
  uint16_t p = buf->Tail;
  buf->Tail = (buf->Tail + 1) & NETBUFMASK;
  return p;
}

// Push a byte in the ring
static void cbpibw(cb_packet_t *buf, uint8_t dat) {
  buf->Buffer[cbpi(buf)] = dat;
}

// Increase Head position and wrap, returning old position
static inline int cbpo(cb_packet_t *buf) {
  uint16_t p = buf->Head;
  buf->Head = (buf->Head + 1) & NETBUFMASK;
  return p;
}

// Pop a byte off the ring
static uint8_t cbpobw(cb_packet_t *buf) {
  return buf->Buffer[cbpo(buf)];
}

// Push a packet in the ring
uint16_t cbpush(cb_packet_t *buf, uint8_t *pkt, uint16_t sze) {
  if(sze >= NETBUFSIZE) return 0;

  cbpibw(buf, ((sze >> 8) & 0xff));
  cbpibw(buf, (sze & 0xff));
  while(sze--) {
    cbpibw(buf, *pkt++);
    // Discard a packet to make room if needed.
    if(buf->Tail == buf->Head) cbpop(buf, NULL, 0);
  }
  return sze;
}

// Pop (or discard) a packet from the ring
uint16_t cbpop(cb_packet_t *buf, uint8_t *pkt, uint16_t msz) {
  uint16_t sze;
  if(pkt && (buf->Tail == buf->Head)) return 0;

  sze = cbpobw(buf) << 8;
  sze|= cbpobw(buf);

  // If there is room, copy the packet, else drop it.
  if((sze <= msz) && (pkt)) {
    for(uint16_t i = 0; i<sze; i++) {
      pkt[i] = cbpobw(buf);
    }
    return sze;
  }

  buf->Head = (buf->Head + sze) & NETBUFMASK;
  return 0;
}

uint16_t target_EthernetSend(uint8_t *pkt, uint16_t sze) {
  if(sze >= NETBUFSIZE) return 0;
  return cbpush(&cPacketOut, pkt, sze);
}

uint16_t target_EthernetRecv(uint8_t *pkt, uint16_t sze) {
  return cbpop(&cPacketIn, pkt, sze);
}


void ethernetInit() {
  W5100.init();
  SPI1.beginTransaction(SPI_ETHERNET_SETTINGS);
  W5100.setMACAddress(mac_addr);
  SPI1.endTransaction();
  W5100.writeSnMR(0, SnMR::MACRAW); 
  W5100.execCmdSn(0, Sock_OPEN);
}

void ethernetSetMAC(uint8_t *newmac) {
  memcpy(mac_addr, newmac, 6);
  mac_addr_service = 1;
}

void ethernetService() {
  uint16_t len;

  if(mac_addr_service) {
    mac_addr_service = 0;
    W5100.setMACAddress(mac_addr);
  }
  
  if(cbpeek(&cPacketOut)) {
    len = cbpop(&cPacketOut, ePacketBuf, sizeof(ePacketBuf));

    // Hand packet off to ethercon
    W5100.send_data_processing(0, ePacketBuf, len);
    W5100.execCmdSn(0, Sock_SEND_MAC);
  }

  len = W5100.getRXReceivedSize(0);
  if(len > 0) {
    // Get packet size from ethercon
    W5100.recv_data_processing(0, ePacketBuf, 2);
    W5100.execCmdSn(0, Sock_RECV);
    len = (ePacketBuf[0] << 8) | ePacketBuf[1];

    // Get packet data from ethercon
    W5100.recv_data_processing(0, ePacketBuf, len);
    W5100.execCmdSn(0, Sock_RECV);

    // Push onto recv buffer, dropping packets as needed
    cbpush(&cPacketIn, ePacketBuf, len);
  }
}

#endif

