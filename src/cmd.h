#ifndef __CMD_H
#define __CMD_H

#define MAX_MSG_SIZE    256
#define MAXARG           32

#include <stdint.h>

// command line structure
typedef struct Commands_s {
  const char *Name;
  const char *ValidPath;
  int MinParams;
  const char *ShortHelp;
  const char *FullHelp;
  void (*Func)(int argc, char **argv);
  struct Commands_s *Dispatch;
} Commands_t;

void cmdDisplay();
void cmdParse(char *cmd);
void cmdPoll();

void cmdCommandHelp(boolean singleCommand, Commands_t *table, int cmd);
void cmdDispatchHelp(Commands_t *table, int argc, char **argv);
void cmdDispatch(Commands_t *table, int argc, char **argv);

int execHandler(char *filename);
int execLoop();

void execcmd(int argc, char **argv);
void showcmd(int argc, char **argv);
void setcmd(int argc, char **argv);

extern Commands_t GlobalCommands[];

#endif /* __CMD_H */
