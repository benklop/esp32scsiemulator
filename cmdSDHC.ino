#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>

Commands_t SDHCCommands[] = {
  { NULL, 0, NULL, NULL }
};

void sdhccmd(int argc, char **argv) {
  cmdDispatch(SDHCCommands, argc, argv);
}

