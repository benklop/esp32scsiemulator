#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>

Commands_t FilesCommands[] = {
  { NULL, 0, NULL, NULL }
};

void filescmd(int argc, char **argv) {
  cmdDispatch(FilesCommands, argc, argv);
}

