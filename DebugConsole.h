extern int verbosity;

#define DEBUG
#ifdef DEBUG
#define DEBUGPRINT(x, ...) { if(x & verbosity) if(Serial) Serial.printf(__VA_ARGS__); }
#define CHECKCONSOLE()  { if(Serial.available()) return 1; }
#define die(text, rc) DEBUGPRINT(DBG_GENERAL, "%s: Failed with rc=%u.\r\n", text, rc)
#else
#define DEBUGPRINT(...) { }
#define CHECKCONSOLE()  { }
#define die(text, rc) { }
#endif

#define DBG_GENERAL  0x001
#define DBG_ERROR    0x002
#define DBG_WARN     0x004
#define DBG_INFO     0x008
#define DBG_TRACE    0x010
#define DBG_LOWLEVEL 0x020
#define DBG_DATABUFS 0x040
#define DBG_DMA      0x080
#define DBG_PIO      0x100
#define DBG_HOLYCRAP 0x200

