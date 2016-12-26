#ifndef SCSI_COMMANDS_H
#define SCSI_COMMANDS_H

/* Mode pages */
#define MODEPAGE_VENDOR_SPECIFIC    0x00
#define MODEPAGE_RW_ERROR_RECOVERY  0x01
#define MODEPAGE_DCRC_PARAMETERS    0x02
#define MODEPAGE_FORMAT_PARAMETERS  0x03
#define MODEPAGE_RIGID_GEOMETRY     0x04
#define MODEPAGE_APPLE              0x30
#define MODEPAGE_ALL_PAGES          0x3F

/* 6 Byte command opcodes */
#define CMD_TEST_UNIT_READY       0x00
#define CMD_REZERO_UNIT           0x01
#define CMD_REQUEST_SENSE         0x03
#define CMD_FORMAT_UNIT           0x04
#define CMD_READ6                 0x08
#define CMD_WRITE6                0x0A  /* Optional */
#define CMD_SEEK6                 0x0B  /* Optional */

#define CMD_INQUIRY               0x12
#define CMD_RESERVE6              0x16  /* Optional */
#define CMD_RELEASE6              0x17  /* Optional */
#define CMD_MODE_SENSE6           0x1A  /* Optional */
#define CMD_START_STOP_UNIT       0x1B  /* Optional */
#define CMD_RECV_DIAGNOSTIC       0x1C
#define CMD_SEND_DIAGNOSTIC       0x1D
/* 10 Byte command opcodes */
#define CMD_READ_CAPACITY10       0x25
#define CMD_READ10                0x28
#define CMD_WRITE10               0x2A  /* Optional */
#define CMD_SEEK10                0x2B  /* Optional */
#define CMD_READUPDATEDBLOCK10    0x2D
#define CMD_WRITEANDVERIFY10      0x2E  /* Optional */
#define CMD_VERIFY10              0x2F
#define CMD_PREFETCH_CACHE10      0x34  /* Optional */
#define CMD_SYNCHRONIZE_CACHE10   0x35  /* Optional */
#define CMD_LOCKUNLOCK_CACHE10    0x36  /* Optional */
#define CMD_READ_DEFECT_DATA      0x37  /* Optional */
#define CMD_WRITEBUFFER           0x3B  /* Optional */
#define CMD_READBUFFER            0x3C  /* Optional */
#define CMD_READLONG10            0x3E  /* Optional */
#define CMD_WRITELONG10           0x3F  /* Optional */
#define CMD_RESERVE10             0x56
#define CMD_RELEASE10             0x57
/* 12 Byte command opcodes */
#define CMD_REPORT_LUNS           0xA0  /* Optional */
#define CMD_MAC_UNKNOWN           0xEE  /* Unknown */

/* Dayna SCSI/Link Ethernet */
#define CMD_SCSILINK_STATS        0x09
#define CMD_SCSILINK_ENABLE       0x0E
#define CMD_SCSILINK_SET          0x0C
#define CMD_SCSILINK_SETMODE      0x80
#define CMD_SCSILINK_SETMAC       0x40

/* Cabletron Ethernet */
#define CMD_CABLETRON_SEND         0x0c01
#define CMD_CABLETRON_RECV         0xe1
#define CMD_CABLETRON_GET_ADDR     0x0c04
#define CMD_CABLETRON_ADD_PROTO    0x0d01
#define CMD_CABLETRON_REM_PROTO    0x0d02
#define CMD_CABLETRON_SET_MODE     0x0d03
#define CMD_CABLETRON_SET_MULTI    0x0d04
#define CMD_CABLETRON_REMOVE_MULTI 0x0d05
#define CMD_CABLETRON_GET_STATS    0x0d06
#define CMD_CABLETRON_SET_MEDIA    0x0d07
#define CMD_CABLETRON_GET_MEDIA    0x0d08
#define CMD_CABLETRON_LOAD_IMAGE   0x0d09
#define CMD_CABLETRON_SET_ADDR     0x0d0a

#endif

