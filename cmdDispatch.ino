#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>

void cmdCommandHelp(Commands_t *table, int cmd) {
  if(!table) {
    DEBUGPRINT(DBG_GENERAL, "ERROR: Command table invalid.\r\n");
    return;
  }
  DEBUGPRINT(DBG_GENERAL, "%s ", table[cmd].Name);
  if(table[cmd].FullHelp) {
    DEBUGPRINT(DBG_GENERAL, "%s\r\n", table[cmd].FullHelp);
  } else if(table[cmd].ShortHelp) {
    DEBUGPRINT(DBG_GENERAL, "\t%s\r\n", table[cmd].ShortHelp);
  }
  if(table[cmd].MinParams) {
    DEBUGPRINT(DBG_GENERAL, "\t(Requires %d Parameter", table[cmd].MinParams);
    if(table[cmd].MinParams>1) DEBUGPRINT(DBG_GENERAL, "s");
    DEBUGPRINT(DBG_GENERAL, ")\r\n");
  }
  DEBUGPRINT(DBG_GENERAL, "\r\n");
}

void cmdDispatchHelp(Commands_t *table, int argc, char **argv) {
  if(argc<2) {
    for(int ii=0; table[ii].Name; ii++) {
      if(strcmp(argv[0], "help"))
        DEBUGPRINT(DBG_GENERAL, "%s ", argv[0]);
      cmdCommandHelp(table, ii);
    }
    return;
  }
  for(int ii=0; table[ii].Name; ii++) {
    if(!strcmp(argv[1], table[ii].Name)) {
      if(table[ii].Dispatch) {
        cmdDispatchHelp(table[ii].Dispatch, argc-1, &argv[1]);
      } else {
        if(strcmp(argv[0], "help"))
          DEBUGPRINT(DBG_GENERAL, "%s ", argv[0]);
        cmdCommandHelp(table, ii);
      }
      return;
    }
  }
}

void cmdDispatch(Commands_t *table, int argc, char **argv) {
  argc--;
  if(argc<1) {
    for(int ii=0; table[ii].Name; ii++) {
      cmdCommandHelp(table, ii);
    }
    return;
  }
  for(int ii=0; table[ii].Name; ii++) {
    if(!strcmp(argv[1], table[ii].Name)) {
      if(argc < table[ii].MinParams) {
        DEBUGPRINT(DBG_GENERAL, "ERROR: Not enough parameters given\r\n");
        cmdCommandHelp(table, ii);
        return;
      }
      table[ii].Func(argc, &argv[1]);
    }
  }
}

void help(int argc, char **argv) {
  cmdDispatchHelp(GlobalCommands, argc, argv);
}

Commands_t GlobalCommands[] = {
  { "show", 1, "[part|luns|sdhc]", NULL, showcmd, ShowCommands },
  { "scsi", 1, "[init|reset|print|target|read|write|id]", NULL, scsicmd, SCSICommands },
//  { "files", 1, "[test]", NULL, filescmd, FilesCommands },
//  { "sdhc", 1, "[test]", NULL, sdhccmd, SDHCCommands },
  { "exec", 1, "<script>", NULL, execcmd, NULL },
  { "echo", 1, "[on|off|message]", NULL, echocmd, NULL },
  { "lun", 1, "[copy]", NULL, luncmd, LUNCommands },
  { "debug", 0, "[dma|verbose|phasedelay]", NULL, debugcmd, DebugCommands },
  { "help", 0, "This help facility.", NULL, help, NULL },
  { NULL, 0, NULL, NULL }
};

void dispatchInit() {
  for(int ii=0; GlobalCommands[ii].Name; ii++) {
    cmdAdd((char*)GlobalCommands[ii].Name, GlobalCommands[ii].Func);
  }
}

