#include "DebugConsole.h"
#include "cmdDispatch.h"
#include <Cmd.h>
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library

extern MicroOLED oled;
char exec_string[512];
int echo_on = 1;

void execInit() {
  memset(exec_string, 0, sizeof(exec_string));
}

int execLoop() {
  if(!exec_string[0]) return 0;
  
  char exec_command[sizeof(exec_string)];
  memset(exec_command, 0, sizeof(exec_string));

  strncpy(exec_command, exec_string, 9);

  oled.rectFill(0, 10, oled.getLCDWidth(), 8, BLACK, NORM);
  oled.setCursor(0,10);
  oled.print("@");
  oled.setCursor(6,10);
  oled.print(exec_command);
  oled.display();

  strncpy(exec_command, exec_string, sizeof(exec_string)-1);
  memset(exec_string, 0, sizeof(exec_string));
  execHandler(exec_command);
  oled.rectFill(0, 10, oled.getLCDWidth(), 8, BLACK, NORM);

  return 1;
}

void execcmd(int argc, char **argv) {
  if(argc > 1) {
    memset(exec_string, 0, sizeof(exec_string));
    strncpy(exec_string, argv[1], sizeof(exec_string)-1);
  }
}

void echocmd(int argc, char **argv) {
  if(argc > 1) {
    if(!strcmp("off", argv[1])) {
      echo_on = 0;
    } else if(!strcmp("on", argv[1])) {
      echo_on = 1;
    } else {
      for(int i = 1; i<argc; i++) {
        DEBUGPRINT(DBG_GENERAL, "%s ", argv[i]);
      }
      DEBUGPRINT(DBG_GENERAL, "\r\n");
    }
  }
}

int execHandler(char *filename) {
  FRESULT rc;         /* Result code */
  FIL fil;            /* File object */
  char exec_cmd[512];

  FATFS fatfs;      /* File system object */
  // 0: is SDHC (configured in diskconfig.h)
  static char device[] = "0:/";

  rc = f_mount (&fatfs, device, 0);
  if (!rc) {
    rc = f_open(&fil, filename, FA_READ | FA_OPEN_EXISTING);
    if(!rc) {
      while(f_gets(exec_cmd, sizeof(exec_cmd)-1, &fil)) {
        for(unsigned int ii = 0; ii<sizeof(exec_cmd); ii++) {
          if(exec_cmd[ii] == '\r') exec_cmd[ii] = 0;
          if(exec_cmd[ii] == '\n') exec_cmd[ii] = 0;
        }
        if(echo_on && exec_cmd[0]!='@')
          DEBUGPRINT(DBG_GENERAL, ">>>%s\r\n", exec_cmd);
        if(exec_cmd[0] != '#')
          cmdParse((exec_cmd[0]!='@') ? exec_cmd : exec_cmd+1);
      }
      f_close(&fil);
    }
    f_mount (NULL, device, 0);
  }
  return 0;
}
