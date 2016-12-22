#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>
#include "drvNCR5380.h"

extern uint8_t unit_attention;

void debug_dma(int argc, char **argv) {
  if(argc > 1) {
    if(!strcmp("off", argv[1])) {
      ncr_dmamode = DMA_NONE;
    } else if(!strcmp("real", argv[1])) {
      ncr_dmamode = DMA_REAL;
    } else if(!strcmp("pseudo", argv[1])) {
      ncr_dmamode = DMA_PSEUDO;
    } else {
      cmdCommandHelp(LUNCommands, 0);
    }
  } else {
    switch(ncr_dmamode) {
      case DMA_REAL:
        DEBUGPRINT(DBG_GENERAL, "DMA Mode: REAL\r\n");
        break;
      case DMA_PSEUDO:
        DEBUGPRINT(DBG_GENERAL, "DMA Mode: PSEUDO\r\n");
        break;
      default:
        DEBUGPRINT(DBG_GENERAL, "DMA Mode: OFF\r\n");
        break;
    }
  }
}

void debug_verbose(int argc, char **argv) {
  if(argc > 1) {
    verbosity = cmdStr2Num(argv[1], 10);
  }
  DEBUGPRINT(DBG_GENERAL, "Verbose: %d\r\n", verbosity);
}


void debug_ua(int argc, char **argv) {
  if(argc > 1) {
    if(!strcmp(argv[1], "off"))
      unit_attention = 0;
  }
  DEBUGPRINT(DBG_GENERAL, "Unit Attention: %d\r\n", unit_attention);
}

void debug_phasedelay(int argc, char **argv) {
  if(argc > 1) {
    phasedelay = cmdStr2Num(argv[1], 10);
  }
  DEBUGPRINT(DBG_GENERAL, "Phase Delay: %d\r\n", phasedelay);
}

void debug_interrupts(int argc, char **argv) {
  if(argc > 1) {
    target_interrupt = cmdStr2Num(argv[1], 10) != 0;
  }
  DEBUGPRINT(DBG_GENERAL, "Target Interrupt: %s\r\n", target_interrupt ? "on" : "off");
}


Commands_t DebugCommands[] = {
  { "ua", 0, "Unit Attention", "off", debug_ua, NULL },
  { "dma", 0, "DMA Mode", "[off|real|pseudo]", debug_dma, NULL },
  { "verbose", 0, "Verbosity", "[GENERAL|ERROR|WARN|INFO|TRACE|LOWLEVEL|HOLYCRAP]", debug_verbose, NULL },
  { "phasedelay", 0, "SCSI Phase Delay", "[off|real|pseudo]", debug_phasedelay, NULL },
  { "interrupts", 0, "SCSI Interrupts", "[0|1]", debug_interrupts, NULL },
  { NULL, 0, NULL, NULL }
};

void debugcmd(int argc, char **argv) {
  cmdDispatch(DebugCommands, argc, argv);
}

