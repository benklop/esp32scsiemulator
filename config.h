#ifndef CONFIG_H
#define CONFIG_H


/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  DEBUGGING                                        X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/

/*****************************************************
 * Enables Serial Console at a minor speed penalty
*/
#define DEBUG


/*****************************************************
 * Wait for Serial Console at startup
*/
#define DEBUG_WAIT




/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  SCSI Controllers - Pick ONE                      X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/

/*****************************************************
 * NCR 5380 SCSI Controller
*/
#define DRIVER_SCSI_NCR5380


/*****************************************************
 * NCR 53C94/53C96 SCSI Controller
*/
// #define DRIVER_SCSI_NCR53C9x




/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  SCSI Roles - Pick ANY                            X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/

/*****************************************************
 * SCSI Initiator
 * Allows use as a SCSI interrogator
 * and as a SCSI disk cloner
*/
#define SCSIROLE_INITIATOR


/*****************************************************
 * SCSI Target
 * Required to emulate disks, cdroms, etc.
*/
#define SCSIROLE_TARGET

 
 
 
 /*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  Screen Controllers - Pick ONE or NONE            X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/

/*****************************************************
 * SparkFun MicroOLED Display on SPI1
 * nCS on Pin31, D/C on Pin28, Reset on Pin27
*/
#define DRIVER_OLED_DISPLAY




/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  Graphics Emulations - Pick ANY                   X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
 
/*****************************************************
 * Radius PowerView Emulation - NOT IMPLEMENTED
*/
// #define SUPPORT_GRAPHICS_POWERVIEW


/*****************************************************
 * ScuzzyGraph Emulation - NOT IMPLEMENTED
*/
// #define SUPPORT_GRAPHICS_SCUZZYGRAPH




/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  Ethernet Controllers - Pick ONE or NONE          X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/

/*****************************************************
 * W5100 Ethernet on SPI1
 * nCS on Pin24
*/
#define DRIVER_ETHERNET_W5100


/*****************************************************
 * Particle WiFi bridge on SPI1 - NOT IMPLEMENTED
 * nCS on Pin24
*/
// #define DRIVER_ETHERNET_PARTICLE




/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  Ethernet Emulations - Pick ANY                   X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
 
/*****************************************************
 * Cabletron EA41x Emulation - NEEDS WORK
*/
// #define SUPPORT_ETHERNET_CABLETRON


/*****************************************************
 * DaynaPort SCSI/Link Emulation - NEEDS WORK
*/
#define SUPPORT_ETHERNET_SCSILINK


/*****************************************************
 * RFC 2143 Emulation - NOT IMPLEMENTED
 * Any device can send packets to any other device
 * requires SCSI Arbitration support (multi-master)
*/
// #define SUPPORT_ETHERNET_RFC2143




/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 X                                                   X
 X  General SCSI Commands - Adjust to suit host      X
 X                                                   X
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
 
/*****************************************************
 * By default all LUNs want attention at startup
 * supposedly some systems need this?
*/
// #define SUPPORT_UNIT_ATTENTION


#endif /* CONFIG_H */

