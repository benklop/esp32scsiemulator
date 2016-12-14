#ifndef CMD_DISPATCH_H
#define CMD_DISPATCH_H

typedef struct Commands_s {
  const char *Name;
  int MinParams;
  const char *ShortHelp;
  const char *FullHelp;
  void (*Func)(int argc, char **argv);
  struct Commands_s *Dispatch;
} Commands_t;

void cmdCommandHelp(Commands_t *table, int cmd);
void cmdDispatchHelp(Commands_t *table, int argc, char **argv);
void cmdDispatch(Commands_t *table, int argc, char **argv);
void dispatchInit();
void execInit();
int execHandler(char *filename);
int execLoop();

void execcmd(int argc, char **argv);
void showcmd(int argc, char **argv);
void scsicmd(int argc, char **argv);
void filescmd(int argc, char **argv);
void sdhccmd(int argc, char **argv);
void luncmd(int argc, char **argv);
void debugcmd(int argc, char **argv);

extern Commands_t ShowCommands[];
extern Commands_t SCSICommands[];
extern Commands_t FilesCommands[];
extern Commands_t SDHCCommands[];
extern Commands_t LUNCommands[];
extern Commands_t DebugCommands[];
extern Commands_t GlobalCommands[];

#endif
