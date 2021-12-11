#include "cmd.h"

// command line message buffer and pointer
static uint8_t msg[MAX_MSG_SIZE] = "";
static uint8_t *msg_ptr = msg;

int echo_on = 1;
int errorlevel = 0;

// Exec script stack, this allows executing scripts from within scripts.
typedef struct script_s {
  char m_filename[MAX_FILE_PATH+1];
  FsFile m_file;
  int m_line;
} script_t;

#define MAX_SCRIPTLEVEL 8
script_t scriptstack[MAX_SCRIPTLEVEL];
int scriptlevel = -1;

#define exec_filename scriptstack[scriptlevel].m_filename
#define exec_line scriptstack[scriptlevel].m_line

char cmd_prefix[256] = "/";

extern uint8_t mbr_bin[];

void cmdDisplay() {
  if(cmd_prefix[0] != 0) {
    Serial.printf("[%s] $ ", cmd_prefix);
  } else {
    strcpy(cmd_prefix, "/");
    Serial.print("[/] $ ");
  }
}

/*
 * Like strtok, but with quotes
 */
char *strmbtok ( char *input, const char *delimit, const char *openblock, const char *closeblock) {
    static char *token = NULL;
    char *lead = NULL;
    char *block = NULL;
    int iBlock = 0;
    int iBlockIndex = 0;

    if ( input != NULL) {
        token = input;
        lead = input;
    }
    else {
        lead = token;
        if ( *token == '\0') {
            lead = NULL;
        }
    }

    while ( *token != '\0') {
        // We are in a quote, is this a matching close quote character?
        if ( iBlock) {
            if ( closeblock[iBlockIndex] == *token) {
                iBlock = 0;
            }
            token++;
            continue;
        }

        // Is this a valid opening quote?
        if ( ( block = strchr ( openblock, *token)) != NULL) {
            iBlock = 1;
            iBlockIndex = block - openblock;
            token++;
            continue;
        }
        if ( strchr ( delimit, *token) != NULL) {
            *token = '\0';
            token++;
            break;
        }
        token++;
    }
    return lead;
}

/*
 * Remove quotes from quoted strings.
 */
void unquote(char *out, char *in) {
  char tmp_str[256];

  if(!in) return;
  if(!out) return;

  char *pout = tmp_str;
  char inQuote = 0;
  while(*in != 0) {
    if(inQuote && (*in == inQuote)) {
      inQuote = 0;
    } else if(!inQuote && (*in == '"')) {
      inQuote = '"';
    } else if(!inQuote && (*in == '\'')) {
      inQuote = '\'';
    } else {
      *pout = *in;
      pout++;
    }
    in++;
  }
  *pout = 0;

  // This way you can have out == in
  strcpy(out, tmp_str);
}

#define ENVIRONMENT_SIZE 32
#define ENVIRONMENT_KEY_SIZE 32
#define ENVIRONMENT_VALUE_SIZE (MAX_FILE_PATH+1)

typedef struct cmdvar_s {
  char key[ENVIRONMENT_KEY_SIZE];
  char value[ENVIRONMENT_VALUE_SIZE];
} cmdvar_t;

cmdvar_t cmdenv[ENVIRONMENT_SIZE];

int getvar(const char *varname, int defaultvalue) {
  if(!strcasecmp(varname, "ERRORLEVEL")) return errorlevel;
  
  for(int i = 0; i < ENVIRONMENT_SIZE; i++) {
    if(!strcmp(cmdenv[i].key, varname)) {
      return strtol(cmdenv[i].value, NULL, 0);
    }
  }
  return defaultvalue;
}

char* getvar(const char *varname, char *defaultvalue) {
  if(!strcasecmp(varname, "ERRORLEVEL")) return NULL;

  for(int i = 0; i < ENVIRONMENT_SIZE; i++) {
    if(!strcasecmp(cmdenv[i].key, varname)) {
      return cmdenv[i].value;
    }
  }
  return defaultvalue;
}

void setvarcmd(int argc, char **argv) {
  int i;
  
  if(argc != 3) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Syntax error in setvar command.\r\n");
    return;
  }

  if(!strcasecmp(argv[1], "ERRORLEVEL")) return;

  // Find existing variable matching name and replace it
  for(i = 0; i < ENVIRONMENT_SIZE; i++) {
    if(!strcasecmp(cmdenv[i].key, argv[1])) {
      strncpy(cmdenv[i].value, argv[2], ENVIRONMENT_VALUE_SIZE);
      cmdenv[i].value[ENVIRONMENT_VALUE_SIZE-1] = 0;

      return;
    }
  }  

  // Create new variable if we have room.
  for(i = 0; i < ENVIRONMENT_SIZE; i++) {
    if(cmdenv[i].key[0] == 0) {
      strncpy(cmdenv[i].key, argv[1], ENVIRONMENT_KEY_SIZE);
      cmdenv[i].key[ENVIRONMENT_KEY_SIZE-1] = 0;

      strncpy(cmdenv[i].value, argv[2], ENVIRONMENT_VALUE_SIZE);
      cmdenv[i].value[ENVIRONMENT_VALUE_SIZE-1] = 0;

      return;
    }
  }

  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Ran out of environment variable space.\r\n");
}

void unsetvarcmd(int argc, char **argv) {
  int i;
  
  if(argc != 2) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Syntax error in unsetvar command.\r\n");
    return;
  }

  if(!strcasecmp(argv[1], "ERRORLEVEL")) return;

  // Find existing variable matching name and delete it
  for(i = 0; i < ENVIRONMENT_SIZE; i++) {
    if(!strcasecmp(cmdenv[i].key, argv[1])) {
      memset(cmdenv[i].key, 0, ENVIRONMENT_KEY_SIZE);
      memset(cmdenv[i].value, 0, ENVIRONMENT_VALUE_SIZE);

      return;
    }
  }

  // Quietly fail if the variable doesn't exist
}

void getvarcmd(int argc, char **argv) {
  int i;
  
  if(argc > 2) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Syntax error in getvar command.\r\n");
    return;
  }

  if(!strcasecmp(argv[1], "ERRORLEVEL")) {
    Serial.printf("%d\r\n", errorlevel);
    return;
  }

  for(i = 0; i < ENVIRONMENT_SIZE; i++) {
    if(!strcasecmp(cmdenv[i].key, argv[1])) {
      Serial.printf("%s=%s\r\n", cmdenv[i].key, cmdenv[i].value);
      return;
    }
  }  

  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": variable '%s' not found.\r\n", argv[1]);
  return;
}

void cmdParse(char *cmd) {
  uint8_t argc, i = 0;
  char *argv[MAXARG];

  // leading whitespace and blank lines are ignored
  while((*cmd == ' ') || (*cmd == '\t')) cmd++;
	if(!cmd[0])	return;

  // ignore labels for regular command parsing
  if(cmd[0] == ':') return;

  // parse the statement and tokenize it (with quote handling)
  argv[i] = strmbtok(cmd, " ", "\"'", "\"'");
  while ((argv[i] != NULL) && (i < MAXARG)) {
    // unquote the previous argument (last argument is always NULL)
    unquote(argv[i], argv[i]);
    argv[++i] = strmbtok(NULL, " ", "\"'", "\"'");
  };
  
  // save the number of arguments
  argc = i;

  while(!strcasecmp(argv[0], "if")) {
    if(argc > 3) {
      int aval = getvar(argv[1], 0);
      int cval = strtol(argv[3], NULL, 0);
  
      if(!strcmp(argv[2], ">")) {
        if(!(aval > cval)) return;
      } else if(!strcmp(argv[2], "<")) {
        if(!(aval < cval)) return;
      } else if(!strcmp(argv[2], ">=")) {
        if(!(aval >= cval)) return;
      } else if(!strcmp(argv[2], "<=")) {
        if(!(aval <= cval)) return;
      } else if(!strcmp(argv[2], "==")) {
        if(!(aval == cval)) return;
      } else if(!strcmp(argv[2], "!=")) {
        if(!(aval != cval)) return;
      } else {
        errorlevel = -1;
        Serial.print("ERROR");
        if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
        Serial.printf(": Unknown if conditional '%s'.\r\n", argv[1]);
        return;
      }
  
      // condition met, parse rest of the line, skipping over "then" if present.
      if(!strcmp(argv[3], "then")) {
        argc -= 4;
        for(int iz=0; iz < argc; iz++) {
          argv[iz] = argv[iz+4];
        }
      } else {
        argc -= 3;
        for(int iz=0; iz < argc; iz++) {
          argv[iz] = argv[iz+3];
        }
      }
    } else {
      errorlevel = -1;
      Serial.print("ERROR");
      if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
      Serial.printf(": Not enough parameters to if command.\r\n");
      return;
    }
  }

  // find the handler responsible for this command
  for(int ii=0; GlobalCommands[ii].Name; ii++) {
    if (!strcmp(argv[0], GlobalCommands[ii].Name)) {
      errorlevel = 0;
      GlobalCommands[ii].Func(argc, argv);
      return;
    }
  }

  // command not recognized. print message and re-generate prompt.
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Unknown command '%s'.\r\n", argv[0]);
}

void cmd_handler() {
  char c = Serial.read();

  switch (c) {
    case '\n':
      break;
    case '\r':
      // terminate the msg and reset the msg ptr. then send
      // it to the handler for processing.
      *msg_ptr = '\0';
      if(msg[0] != 0) {
        Serial.print("\r\n");
        cmdParse((char *)msg);
        execLoop();
        cmdDisplay();
      } else {
        Serial.print("\r\n");
        cmdDisplay();
      }
      msg_ptr = msg;
      break;
  
    case '\b':
      // backspace 
      if (msg_ptr > msg)
      {
          *msg_ptr = '\0';
          Serial.print(c);
          msg_ptr--;
      } else {
          Serial.print('\x07');
          msg_ptr = msg;
          msg[0] = 0;
      }
      break;
  
    default:
      // normal character entered. add it to the buffer
      if(msg_ptr < (msg + sizeof(msg) - 1)) {
        Serial.print(c);
        *msg_ptr++ = c;
        *msg_ptr = '\0';
      } else {
        Serial.print('\x07');
      }
      break;
  }
}

void cmdPoll() {
  if (Serial.available()) {
    cmd_handler();
  }
}

void gotocmd(int argc, char **argv) {
  fspos_t tmp_pos;
  int tmp_line = 1;
  char tmp_cmd[256];
  if(scriptlevel < 0) {
    errorlevel = -1;
    Serial.printf("ERROR: goto is only valid inside a script.\r\n");
    return;
  }

  // Save our current position in case we can't find the label (should we exit the script on goto failure?)
  scriptstack[scriptlevel].m_file.fgetpos(&tmp_pos);

  // Rewind to the top of the script in case the label is up instead of down.
  scriptstack[scriptlevel].m_file.rewind();

  // Find the label
  while(scriptstack[scriptlevel].m_file.available()) {
    int n = scriptstack[scriptlevel].m_file.fgets(tmp_cmd, sizeof(tmp_cmd)-1, NULL);
    if (n <= 0) {
      Serial.printf("Reading script '%s' failed at line %d.\r\n", exec_filename, exec_line);
      
      break;
    }

    if(tmp_cmd[0] == ':') {
      if(!strcmp(tmp_cmd+1, argv[1])) {
        // Found it, script continues on the next line after the label. Update the line counter so any errors show the correct line reference.
        exec_line = tmp_line;
        return;
      }
    }
    tmp_line++;
  }

  // We couldn't find the label, restore the file position.
  scriptstack[scriptlevel].m_file.fsetpos(&tmp_pos);

  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Label '%s' was not found.\r\n", argv[1]);
}

void exitcmd(int argc, char **argv) {
  if(scriptlevel < 0) {
    errorlevel = -1;
    Serial.printf("ERROR: exit is only valid inside a script.\r\n");
    return;
  }

  scriptstack[scriptlevel].m_file.close();
  exec_line = 0;
  scriptlevel--;
}

int execLoop() {
  char exec_cmd[256];
  while(scriptlevel >= 0) {
    if(scriptstack[scriptlevel].m_file.available()) {
      int n = scriptstack[scriptlevel].m_file.fgets(exec_cmd, sizeof(exec_cmd)-1, NULL);
      if (n <= 0) {
        Serial.printf("Reading script '%s' failed at line %d.\r\n", exec_filename, exec_line);
        goto close_file;
      }
      exec_cmd[sizeof(exec_cmd)-1] = 0;
      for(int ii = 0; ii < n; ii++) {
        if(exec_cmd[ii] == '\r') exec_cmd[ii] = 0;
        if(exec_cmd[ii] == '\n') exec_cmd[ii] = 0;
      }

      if(echo_on && exec_cmd[0]!='@') {
        cmdDisplay();
        Serial.printf("%s\r\n", exec_cmd);
      }

      if(exec_cmd[0] != '#')
        cmdParse((exec_cmd[0]!='@') ? exec_cmd : exec_cmd+1);

      exec_line++;
    } else {
close_file:
      scriptstack[scriptlevel].m_file.close();
      exec_line = 0;
      scriptlevel--;
    }
  }

  return 0;
}

void execscript(const char *script) {
  char tmp_path[MAX_FILE_PATH+1];
  char *filename = tmp_path;

  fixupPath(tmp_path, script);

  if(!strncmp(filename, "/sd/", 4)) {
    filename += 3;

    if(!sd.exists(filename)) {
      errorlevel = -1;
      Serial.print("ERROR");
      if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
      Serial.printf(": file '%s' not found.\r\n", script);
      return;
    }
  } else {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": file '%s' not found.\r\n", script);
    return;
  }

  if(scriptlevel >= MAX_SCRIPTLEVEL-1) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Script stack space exhausted.\r\n");
    return;
  }

  scriptlevel++;
  scriptstack[scriptlevel].m_file = sd.open(filename, O_RDONLY);
  if(scriptstack[scriptlevel].m_file.isOpen()) {
    strcpy(exec_filename, script);
    exec_line = 1;
  } else {
    scriptlevel--;
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Unable to open script '%s'.\r\n", script);
    return;
  }
}

void execcmd(int argc, char **argv) {
  if(argc > 1) {
    execscript(argv[1]);
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
        Serial.printf("%s ", argv[i]);
      }
      Serial.printf("\r\n");
    }
  }
}

void cmdCommandHelp(boolean singleCommand, Commands_t *table, int cmd) {
  if(!table) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Command table invalid.\r\n");
    return;
  }
  if(singleCommand && table[cmd].FullHelp) {
    Serial.print(table[cmd].FullHelp);
  } else if(table[cmd].ShortHelp) {
    Serial.print("\r\n");
    Serial.print(table[cmd].Name);
    for(uint8_t i = 0; i < (16 - strlen(table[cmd].Name)); i++) {
      Serial.print(" ");
    }
    Serial.printf("%s\r\n", table[cmd].ShortHelp);
    if(table[cmd].MinParams) {
      Serial.printf("                (Requires %d %s)\r\n", table[cmd].MinParams, (table[cmd].MinParams>1) ? "Parameters" : "Parameter");
    }
    if(singleCommand)
      Serial.print("\r\n");
  }
}

void cmdDispatchHelp(Commands_t *table, int argc, char **argv) {
  if(!table) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": invalid command table.\r\n");
    return;
  }
  if(argc<2) {
    for(int ii=0; table[ii].Name; ii++) {
      cmdCommandHelp(false, table, ii);
    }
    Serial.print("\r\n");
    return;
  }
  for(int ii=0; table[ii].Name; ii++) {
    if(!strcmp(argv[1], table[ii].Name)) {
      if(table[ii].Dispatch) {
        cmdDispatchHelp(table[ii].Dispatch, argc-1, &argv[1]);
      } else {
        cmdCommandHelp(true, table, ii);
      }
      return;
    }
  }
}

void clearcmd(int argc, char **argv) {
  Serial.printf("\x1b[2J\x1b[H");
}

void help(int argc, char **argv) {
  cmdDispatchHelp(GlobalCommands, argc, argv);
}

void cmdDispatch(Commands_t *table, int argc, char **argv) {
  int ii;

  if(!table) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Command table invalid.\r\n");
    return;
  }

  argc--;
  if(argc<1) {
    for(ii=0; table[ii].Name; ii++) {
      cmdCommandHelp(false, table, ii);
    }
    return;
  }
  for(ii=0; table[ii].Name; ii++) {
    if(!strcmp(argv[1], table[ii].Name)) {
      if(argc < table[ii].MinParams) {
        errorlevel = -1;
        Serial.print("ERROR");
        if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
        Serial.printf(": Not enough parameters given\r\n");
        return;
      }
      table[ii].Func(argc, &argv[1]);
    }
  }
}

void fixupPath(char *out, const char *in) {
  char *parent;

  char tmp_path[MAX_FILE_PATH+1];

  if(!in) return;
  if(!out) return;

  //unquote(tmp_path, in);
  strcpy(tmp_path, in);

  // Skip leading "./" if present
  if(!strncmp(tmp_path, "./", 2)) {
    sprintf(out, "%s/%s", cmd_prefix, tmp_path+2);
  } else if(!strcmp(tmp_path, "..")) {
    // Ascend if ".."
    strcpy(tmp_path, cmd_prefix);
    parent = strrchr(tmp_path, '/');
    if(parent && (tmp_path != parent)) {
      parent[0] = 0;
    }
    strcpy(out, tmp_path);
  } else if(*tmp_path == '/') {
    // Fully qualified path, replace everything
    strcpy(out, tmp_path);
  } else {
    // Relative path, append
    parent = strrchr(cmd_prefix, '/');
    if(parent[1] != 0) {
      sprintf(out, "%s/%s", cmd_prefix, tmp_path);
    } else {
      sprintf(out, "%s%s", cmd_prefix, tmp_path);
    }
  }

  // Make sure there is not a trailing /
  parent = strrchr(out, '/');
  if((parent != out) && (parent[1] == 0)) {
    parent[0] = 0;
  }
}

// Remove any command prefix levels
void changeDirectory(int argc, char **argv) {
  char new_prefix[256];
  char *suffix = NULL;

  if(argc>=2) {
    // Parse the path string
    fixupPath(new_prefix, argv[1]);

    if(!strncmp("/vdevs/vdev", new_prefix, 11)) {
      char *vdev_name = new_prefix + 11;
      uint8_t v = strtol(vdev_name, &suffix, 0);
      if((v < NUM_VDEV) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
        sprintf(cmd_prefix, "/vdevs/vdev%d", v);
        return;
      }
      goto e_invalidpath;
    }

    if(!strncmp("/tgts/tgt", new_prefix, 9)) {
      char *tgt_name = new_prefix + 9;
      char *lun_name = NULL;
      uint8_t id = strtol(tgt_name, &lun_name, 0);
      if(id < NUM_SCSIID) {
        if(!lun_name || (lun_name[0] == 0) || !strcmp(lun_name, "/")) {
          sprintf(cmd_prefix, "/tgts/tgt%d", id);
          return;
        } else if(!strncmp("/lun", lun_name, 4)) {
          lun_name += 4;
          uint8_t lun = strtol(lun_name, &suffix, 0);
          if((lun < NUM_SCSILUN) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
            sprintf(cmd_prefix, "/tgts/tgt%d/lun%d", id, lun);
            return;
          }
        }
      }
      goto e_invalidpath;
    }

    if(!strcmp(new_prefix, "/") || !strcmp(new_prefix, "/tgts") || !strcmp(new_prefix, "/vdevs")
       || !strcmp(new_prefix, "/sd") || !strcmp(new_prefix, "/diag")|| !strcmp(new_prefix, "/diag/sd")
    ) {
      strcpy(cmd_prefix, new_prefix);
      return;
    } else if(!strncmp(new_prefix, "/sd/", 4)) {
      if((new_prefix[4] == 0) || sd.exists(new_prefix+3)) {
        strcpy(cmd_prefix, new_prefix);
        return;
      }
    }

e_invalidpath:
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Path '%s' not found.\r\n", new_prefix);
  } else {
    Serial.printf("%s\r\n", cmd_prefix);
  }
}

char tmp_left[256];
char tmp_right[256];

void printDirectory(int level, uint8_t lines, const char *l, const char *r) {
  for(int pad = 0; pad < level; pad++) {
    if(lines & (1 << (level - pad))) {
      Serial.print("| ");
    } else {
      Serial.print("  ");
    }
  }
  Serial.print("o- ");
  int dots = 78 - ((level*2) + 3 + strlen(l) + strlen(r));
  Serial.print(l);
  while(dots--) {
    Serial.print(".");
  }
  Serial.print(r);
  Serial.print("\r\n");
}

void showVDEV(int level, uint8_t lines, uint8_t v) {
  VirtualDevice_t *h = &m_vdev[v];
  if(h->m_enabled) {
    sprintf(tmp_left, "vdev%d/ ", v);
    printDirectory(level, lines, tmp_left, " [...]");

    level++;
    lines<<=1;
    lines|=1;

    switch(h->m_type) {
      case DEV_DISK:
        strcpy(tmp_right, " [disk]");
        break;
      case DEV_OPTICAL:
        strcpy(tmp_right, " [optical]");
        break;
      case DEV_TAPE:
        strcpy(tmp_right, " [tape]");
        break;
      default:
        strcpy(tmp_right, " [unknown]");
        break;
    }
    printDirectory(level, lines, "$type ", tmp_right);

    if((h->m_id < NUM_SCSIID) && (h->m_lun < NUM_SCSILUN)) {
      sprintf(tmp_right, " [/tgts/tgt%d/lun%d]", h->m_id, h->m_lun);
      printDirectory(level, lines, "$mapping", tmp_right);
    }
    if(h->m_file) {
      sprintf(tmp_right, " [%s]", h->m_filename);
      printDirectory(level, lines, "$filename ", tmp_right);
    }
    if((h->m_fileSize) && (h->m_blocksize)) {
      sprintf(tmp_right, " [%llu]", h->m_fileSize / h->m_blocksize);
      printDirectory(level, lines, "$blocks ", tmp_right);
    }
    if(h->m_blocksize) {
      sprintf(tmp_right, " [%d]", h->m_blocksize);
      printDirectory(level, lines, "$blocksize ", tmp_right);
    }
  }
}

void showLUN(int level, uint8_t lines, uint8_t id, uint8_t lun) {
  if(!level) {
    sprintf(tmp_left, "/tgts/tgt%d/lun%d/ ", id, lun);
  } else {
    sprintf(tmp_left, "lun%d/ ", lun);
  }
  uint8_t v = m_vdevmap[id][lun];
  if(v < NUM_VDEV) {
    printDirectory(level, lines, tmp_left, " [...]");

    level++;
    lines <<= 1;
    lines |= 1;
    sprintf(tmp_right, " [/vdevs/vdev%d]", v);
    printDirectory(level, lines, "$mapping", tmp_right);
  }
}

void showTarget(int level, uint8_t lines, uint8_t id) {
  if(!level) {
    sprintf(tmp_left, "/tgts/tgt%d/ ", id);
  } else {
    sprintf(tmp_left, "tgt%d/ ", id);
  }
  if(scsi_id_mask & (1<<id)) {
    printDirectory(level, lines, tmp_left, " [...]");

    level++;
    lines <<= 1;
    lines |= 1;
    for(uint8_t lun = 0; lun < NUM_SCSILUN; lun++) {
      showLUN(level, lines, id, lun);
    }
  } else {
    printDirectory(level, lines, tmp_left, " [DISABLED]");
  }
}

void showDirectory(int argc, char **argv) {
  char *local_prefix = cmd_prefix;
  char tmp_prefix[256];
  char *suffix = NULL;
  uint8_t v;

  if(argc == 2) {
    fixupPath(tmp_prefix, argv[1]);
    local_prefix = tmp_prefix;
  }
  if(!strcmp(local_prefix, "/")) {
    printDirectory(0, 0, "/ ", " [...]");
    printDirectory(1, 0, "diag/ ", " [...]");
    printDirectory(1, 0, "sd/ ", " [...]");
    sprintf(tmp_right, " [%d Target%s]", NUM_SCSIID, (NUM_SCSIID != 1) ? "s" : "");
    printDirectory(1, 0, "tgts/ ", tmp_right);
    sprintf(tmp_right, " [%d Storage Object%s]", m_vdevcnt, (m_vdevcnt != 1) ? "s" : "");
    printDirectory(1, 0, "vdevs/ ", tmp_right);
    return;
  }
  if(!strcmp(local_prefix, "/diag")) {
    printDirectory(0, 0, "/diag/ ", " [...]");
    printDirectory(1, 0, "sd/ ", " [...]");
    return;
  }
  if(!strcmp(local_prefix, "/diag/sd")) {
    printDirectory(0, 0, "/diag/sd/ ", " [...]");

    sprintf(tmp_right, " [SdFat %s]", SD_FAT_VERSION_STR);
    printDirectory(1, 0, "$version ", tmp_right);

    switch(SDFAT_FILE_TYPE) {
      case 0:
        strcpy(tmp_right, " [FAT16]");
        break;
      case 1:
        strcpy(tmp_right, " [FAT16/FAT32]");
        break;
      case 2:
        strcpy(tmp_right, " [ExFAT]");
        break;
      case 3:
        strcpy(tmp_right, " [FAT16/FAT32/ExFAT]");
        break;
    }
    printDirectory(1, 0, "$filesystems ", tmp_right);

    sprintf(tmp_right, " [%d]", MAX_FILE_PATH);
    printDirectory(1, 0, "$max_filename_length ", tmp_right);

    csd_t sd_csd;
    if(sd.card()->readCSD(&sd_csd))
    {
      switch (sd.card()->type()) {
        case SD_CARD_TYPE_SD1:
          strcpy(tmp_right, " [SD1]");
          break;
        case SD_CARD_TYPE_SD2:
          strcpy(tmp_right, " [SD2]");
          break;
        case SD_CARD_TYPE_SDHC:
          if (sdCardCapacity(&sd_csd) < 70000000) {
            strcpy(tmp_right, " [SDHC]");
          } else {
            strcpy(tmp_right, " [SDXC]");
          }
          break;
        default:
          strcpy(tmp_right, " [unknown]");
          break;
      }
      printDirectory(1, 0, "$card_type ", tmp_right);

      uint32_t cardsize = 0.000512 * sdCardCapacity(&sd_csd);
      if(cardsize >= 10000) {
        sprintf(tmp_right, " [%lu GB]", cardsize);
      } else {
        sprintf(tmp_right, " [%lu MB]", cardsize);
      }
      printDirectory(1, 0, "$card_size ", tmp_right);
    }
    cid_t sd_cid;
    if(sd.card()->readCID(&sd_cid))
    {
      sprintf(tmp_right, " [0x%02X]", sd_cid.mid);
      printDirectory(1, 0, "$card_manufacturer ", tmp_right);

      sprintf(tmp_right, " [%c%c]", sd_cid.oid[0], sd_cid.oid[1]);
      printDirectory(1, 0, "$card_oem ", tmp_right);

      sprintf(tmp_right, " [%c%c%c%c%c]", sd_cid.pnm[0], sd_cid.pnm[1], sd_cid.pnm[2], sd_cid.pnm[3], sd_cid.pnm[4]);
      printDirectory(1, 0, "$card_name ", tmp_right);

      sprintf(tmp_right, " [%d.%d]", sd_cid.prv_n, sd_cid.prv_m);
      printDirectory(1, 0, "$card_version ", tmp_right);

      sprintf(tmp_right, " [%02d/20%02d]", sd_cid.mdt_month, (sd_cid.mdt_year_high << 4) | sd_cid.mdt_year_low);
      printDirectory(1, 0, "$card_date ", tmp_right);

      sprintf(tmp_right, " [%08lX]", sd_cid.psn);
      printDirectory(1, 0, "$card_serial ", tmp_right);
    }
    return;
  }  
  if(!strcmp(local_prefix, "/tgts")) {
    sprintf(tmp_right, " [%d Target%s]", NUM_SCSIID, (NUM_SCSIID != 1) ? "s" : "");
    printDirectory(0, 0, "/tgts/ ", tmp_right);
    for(uint8_t id = 0; id < NUM_SCSIID; id++) {
      showTarget(1, 1, id);
    }
    return;
  }
  if(!strncmp(local_prefix, "/tgts/tgt", 9)) {
    char *tgt_name = local_prefix + 9;
    char *lun_name = NULL;
    uint8_t id = strtol(tgt_name, &lun_name, 0);
    if(id < NUM_SCSIID) {
      if(!lun_name || (lun_name[0] == 0) || !strcmp(lun_name, "/")) {
        showTarget(0, 0, id);
        return;
      } else if(!strncmp("/lun", lun_name, 4)) {
        lun_name += 4;
        uint8_t lun = strtol(lun_name, &suffix, 0);
        if((lun < NUM_SCSILUN) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
          showLUN(0, 0, id, lun);
          return;
        }
      }
    }
  }
  
  if(!strcmp(local_prefix, "/vdevs")) {
    sprintf(tmp_right, " [%d Storage Objects]", m_vdevcnt);
    printDirectory(0, 0, "/vdevs/ ", tmp_right);
    for(v = 0; v < NUM_VDEV; v++) {
      showVDEV(1, 1, v);
    }
    return;
  }
  if(!strncmp(local_prefix, "/vdevs/vdev", 11)) {
    char *vdev_name = local_prefix + 11;
    v = strtol(vdev_name, &suffix, 0);
    if((v < NUM_VDEV) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
      showVDEV(0, 0, v);
      return;
    }
  }
  if(!strcmp(local_prefix, "/sd") || !strncmp(local_prefix, "/sd/", 4)) {
    char name[MAX_FILE_PATH+1];
    SdFile root;
    SdFile file;
    if(!strcmp(local_prefix, "/sd"))
      strcat(local_prefix, "/");
    root.open(local_prefix+3);
    if(!root.isDir()) {
      sprintf(tmp_left, "%s ", local_prefix);
      if(root.fileSize() >= 2000000000) {
        sprintf(tmp_right, " [%llu GB]", root.fileSize() / 1000000000);
      } else if(root.fileSize() >= 2000000) {
        sprintf(tmp_right, " [%llu MB]", root.fileSize() / 1000000);
      } else if(root.fileSize() >= 2000) {
        sprintf(tmp_right, " [%llu KB]", root.fileSize() / 1000);
      } else {
        sprintf(tmp_right, " [%llu Bytes]", root.fileSize());
      }
      printDirectory(0, 0, tmp_left, tmp_right);
      root.close();
      return;
    }
    sprintf(tmp_left, "%s ", local_prefix);
    printDirectory(0, 0, tmp_left, " [...]");

    while(file.openNext(&root, O_READ)) {
      file.getName(name, MAX_FILE_PATH+1);
      if(file.isDir()) {
        sprintf(tmp_left, "%s/ ", name);
        printDirectory(1, 0, tmp_left, " [...]");
      } else {
        sprintf(tmp_left, "%s ", name);
        if(file.fileSize() >= 2000000000) {
          sprintf(tmp_right, " [%llu GB]", file.fileSize() / 1000000000);
        } else if(file.fileSize() >= 2000000) {
          sprintf(tmp_right, " [%llu MB]", file.fileSize() / 1000000);
        } else if(file.fileSize() >= 2000) {
          sprintf(tmp_right, " [%llu KB]", file.fileSize() / 1000);
        } else {
          sprintf(tmp_right, " [%llu Bytes]", file.fileSize());
        }
        printDirectory(1, 0, tmp_left, tmp_right);
      }
      file.close();
    }
    root.close();
    return;
  }

  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Path '%s' not found.\r\n", local_prefix);
}

void unlinkcmd(int argc, char **argv) {
  FsFile file;            /* File object */
  char tmp_path[MAX_FILE_PATH+1];
  char *vdev_name = NULL;
  char *tgt_name = NULL;
  char *lun_name = NULL;
  char *suffix = NULL;
  uint8_t v = 0xff;
  uint8_t id = 0xff;
  uint8_t lun = 0xff;
  VirtualDevice_t *h = NULL;
  
  if(argc < 2) {
    return;
  }

  fixupPath(tmp_path, argv[1]);

  if(!strncmp("/tgts/tgt", tmp_path, 9)) {
    tgt_name = tmp_path + 9;
    id = strtol(tgt_name, &lun_name, 0);
    if(id < NUM_SCSIID) {
      if((lun_name) && !strncmp("/lun", lun_name, 4)) {
        // Disable a specific LUN
        lun_name += 4;
        lun = strtol(lun_name, &suffix, 0);
        if((lun < NUM_SCSILUN) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
          v = m_vdevmap[id][lun];
          if(v < NUM_VDEV) {
            h = &m_vdev[v];
            h->m_id = 0xff;
            h->m_lun = 0xff;
            m_vdevmap[id][lun] = 0xff;
            return;
          }
        }
      } else if(!lun_name || (lun_name[0] == 0) || !strcmp(lun_name, "/")) {
        // Disable this target and all LUNs under it
        scsi_id_mask &= ~(1 << id);
        for(lun = 0; lun < NUM_SCSILUN; lun++) {
          v = m_vdevmap[id][lun];
          if(v < NUM_VDEV) {
            h = &m_vdev[v];
            h->m_id = 0xff;
            h->m_lun = 0xff;
            m_vdevmap[id][lun] = 0xff;
          }
        }
        return;
      }
    }
  }
  if(!strncmp("/vdevs/vdev", tmp_path, 11)) {
    vdev_name = tmp_path + 11;
    v = strtol(vdev_name, &suffix, 0);
    if((v < NUM_VDEV) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
      id = h->m_id;
      lun = h->m_lun;
      
      h = &m_vdev[v];
      h->m_id = 0xff;
      h->m_lun = 0xff;

      if((id < NUM_SCSIID) && (lun < NUM_SCSILUN))
        m_vdevmap[id][lun] = 0xff;
      
      return;
    }
  }

  if(!strncmp(tmp_path, "/sd/", 4)) {
    sd.remove(tmp_path+3);
    return;
  }

  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Path '%s' not found.\r\n", tmp_path);
}

void makeimagecmd(int argc, char **argv) {
  FsFile file;
  char tmp_path[MAX_FILE_PATH+1];
  uint64_t fileSize = 0;
  
  if(argc < 4) {
    return;
  }

  fixupPath(tmp_path, argv[1]);
  if(strncmp(tmp_path, "/sd/", 4)) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Can only create images on the SD Card.\r\n");
    return;
  }

  if(!strcmp(argv[2], "msdos") || !strcmp(argv[2], "generic")) {
    char *suffix = NULL;
    fileSize = strtoul(argv[3], &suffix, 0);
    if(suffix && suffix[0] != 0) {
      if(!strcmp(suffix, "KB")) fileSize *= 1000ull;
      if(!strcmp(suffix, "KiB")) fileSize *= 1024ull;
      if(!strcmp(suffix, "MB")) fileSize *= 1000000ull;
      if(!strcmp(suffix, "MiB")) fileSize *= 1024ull * 1024ull;
      if(!strcmp(suffix, "GB")) fileSize *= 1000000000ull;
      if(!strcmp(suffix, "GiB")) fileSize *= 1024ull * 1024ull * 1024ull;
    }

    if(sd.exists(tmp_path+3)) {
      errorlevel = -1;
      Serial.print("ERROR");
      if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
      Serial.printf(": file '%s' already exists.\r\n", tmp_path);
      return;
    }
    if(fileSize < (5ull * 1024ull * 1024ull)) {
      errorlevel = -1;
      Serial.print("ERROR");
      if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
      Serial.printf(": Image would be less than 5 MiB.\r\n");
      return;
    }
    if(fileSize > (2048ull * 1024ull * 1024ull)) {
      errorlevel = -1;
      Serial.print("ERROR");
      if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
      Serial.printf(": Image would be larger than 2 GiB.\r\n");
      return;
    }

    // Fixup image size to our 64 Head 32 Sector X Cylinders formula
    uint64_t cyl = fileSize / (512ull * 64ull * 32ull);
    if(fileSize & 0xFFFFF) cyl++;
    if(cyl > 2048) cyl = 2048;
    fileSize = cyl * (512ull * 64ull * 32ull);

    if(!file.open(tmp_path+3, O_WRONLY | O_CREAT | O_TRUNC)) {
      errorlevel = -1;
      Serial.print("ERROR");
      if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
      Serial.printf(": Unable to open '%s'.\r\n", tmp_path);
    } else {
      // Take advantage of our cylinders being 1MB
      if(!file.preAllocate(cyl)) {
        file.close();
        sd.remove(tmp_path+3);

        errorlevel = -1;
        Serial.print("ERROR");
        if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
        Serial.printf(": Pre-allocate failed, SD Card must be formatted as ExFat.\r\n");
        return;
      }

      if(!strcmp(argv[2], "msdos")) {
        file.write(mbr_bin, 512);
      }
      file.close();

      return;
    }
  }

  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Unable to process command as requested.\r\n");
  return;
}

void catcmd(int argc, char **argv) {
  FsFile file;            /* File object */
  char tmp_path[MAX_FILE_PATH+1];
  char tmp_str[256];
  char *filename = tmp_path;
  
  if(argc < 2) {
    return;
  }

  fixupPath(tmp_path, argv[1]);
  if(!strncmp(filename, "/sd/", 4)) {
    filename += 3;
  } else {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": file '%s' not found.\r\n", tmp_path);
    return;
  }

  if(!sd.exists(filename)) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": file '%s' not found.\r\n", tmp_path);
    return;
  }

  if(!file.open(filename, O_RDONLY)) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Unable to open '%s'.\r\n", tmp_path);
  } else {
    while(file.available()) {
      int n = file.fgets(tmp_str, sizeof(tmp_str)-1, NULL);
      if (n <= 0) {
        errorlevel = -1;
        Serial.print("ERROR");
        if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
        Serial.printf(": Reading '%s' failed.\r\n", tmp_path);
        
        break;
      }

      Serial.print(tmp_str);
    }
    file.close();
  }
}

void enablecmd(int argc, char **argv) {
  uint8_t id = 0xff;
  char *tgt_name = NULL;
  char *suffix;
  char tmp_path[MAX_FILE_PATH+1];

  // The logic here is slightly complicated to allow specifying enable /tgts/tgtN from anywhere, but otherwise require we are in /tgts
  if((argc<2) && strncmp("/tgts", cmd_prefix, 5)) {
    goto e_invalidpath;
  }

  if((argc<2) && !strcmp("/tgts", cmd_prefix)) {
    goto e_notarget;
  }

  if(argc<2) {
    strcpy(tmp_path, cmd_prefix);
  } else {
    fixupPath(tmp_path, argv[1]);
  }

  if(strncmp("/tgts/", tmp_path, 6)) {
    goto e_invalidpath;
  }
  if(!strncmp("/tgts/tgt", tmp_path, 9)) {
    tgt_name = tmp_path + 9;
    id = strtol(tgt_name, &suffix, 0);
  }

  if(id < NUM_SCSIID) {
    scsi_id_mask |= (1<<id);
    return;
  }

//e_invalidtarget:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": Invalid target specified.\r\n");
  return;

e_invalidpath:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": The enable command is not valid in this context.\r\n");
  return;

e_notarget:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": No target specified.\r\n");
  return;
}

void setcmd(int argc, char **argv) {
  uint8_t v = 0xff;
  char *vdev_name = NULL;
  char *param_name = NULL;

  char tmp_path[MAX_FILE_PATH+1];

  if(argc<2) {
    goto e_syntax;
  }

  fixupPath(tmp_path, argv[1]);

  // Allow specifying set /vdevs/vdevN/parameter from anywhere, but otherwise require we are in /vdevs/vdevN
  if(!strncmp("/global", tmp_path, 7)) {
    param_name = tmp_path + 7;

    if(!strcasecmp(param_name, "/quirks")) {
      if(argc<3) {
        Serial.printf("0x%02x\r\n", default_quirks);
      } else {
        default_quirks = strtol(argv[2], NULL, 0);
      }
      return;
    }
  }
  
  // Allow specifying set /vdevs/vdevN/parameter from anywhere, but otherwise require we are in /vdevs/vdevN
  if(!strncmp("/vdevs/vdev", tmp_path, 11)) {
    vdev_name = tmp_path + 11;
    v = strtol(vdev_name, &param_name, 0);

    if(v < NUM_VDEV) {
      VirtualDevice_t *h = &m_vdev[v];

      if((param_name) && !strcasecmp(param_name, "/blocksize")) {
        if(argc<3) {
          Serial.printf("%d\r\n", h->m_blocksize);
        } else {
          h->m_blocksize = strtol(argv[2], NULL, 0);
        }
        return;
      }

      if((param_name) && !strcasecmp(param_name, "/quirks")) {
        if(argc<3) {
          Serial.printf("0x%02x\r\n", h->m_quirks);
        } else {
          h->m_quirks = strtol(argv[2], NULL, 0);
        }
        return;
      }

      if((param_name) && !strcasecmp(param_name, "/type")) {
        if(argc<3) {
          switch(h->m_type) {
            case DEV_DISK:
              strcpy(tmp_right, " [disk]");
              break;
            case DEV_OPTICAL:
              strcpy(tmp_right, " [optical]");
              break;
            case DEV_TAPE:
              strcpy(tmp_right, " [tape]");
              break;
            default:
              strcpy(tmp_right, " [unknown]");
              break;
          }
          printDirectory(0, 0, "$type ", tmp_right);
        } else {
#if SUPPORT_DISK
          if(!strcasecmp(argv[2], "disk")) {
            ConfigureDisk(h, NULL);
            return;
          }
#endif /* SUPPORT_DISK */
#if SUPPORT_OPTICAL
          if(!strcasecmp(argv[2], "optical") || !strcasecmp(argv[2], "cdrom")) {
            ConfigureOptical(h, NULL);
            return;
          }
#endif /* SUPPORT_OPTICAL */
#if SUPPORT_TAPE
          if(!strcasecmp(argv[2], "tape")) {
            ConfigureTape(h, NULL);
            return;
          }
#endif /* SUPPORT_TAPE */
        }
        goto e_invalidvalue;
      }

      goto e_invalidparam;
    }
    goto e_invalidpath;
  }

e_invalidpath:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": The set command is not valid in this context.\r\n");
  return;

e_invalidparam:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Invalid parameter '%s' specified.\r\n", param_name);
  return;

e_invalidvalue:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Invalid value '%s' for parameter '%s' specified.\r\n", argv[2], param_name);
  return;

e_syntax:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": Command syntax violation.\r\n");
  return;
}

void createcmd(int argc, char **argv) {
  if(strncmp("/vdev", cmd_prefix, 5)) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.print(": The create command is not valid in this context.\r\n");
    return;
  }

  if(m_vdevcnt >= NUM_VDEV) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.print(": Virtual Device pool exhausted.\r\n");
    return;
  }

  if(argc<2) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.print(": No device type specified.\r\n");
    return;
  }

  uint8_t v = m_vdevcnt;
  VirtualDevice_t *h = &m_vdev[v];

  h->m_quirks = default_quirks;

#if SUPPORT_DISK
  if(!strcasecmp(argv[1], "disk")) {
    ConfigureDisk(h, NULL);
    goto success;
  }
#endif /* SUPPORT_DISK */
#if SUPPORT_OPTICAL
  if(!strcasecmp(argv[1], "optical") || !strcasecmp(argv[1], "cdrom")) {
    ConfigureOptical(h, NULL);
    goto success;
  }
#endif /* SUPPORT_OPTICAL */
#if SUPPORT_TAPE
  if(!strcasecmp(argv[1], "tape")) {
    ConfigureTape(h, NULL);
    goto success;
  }
#endif /* SUPPORT_TAPE */

//failure:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": Unknown device type specified.\r\n");
  return;

success:
  sprintf(cmd_prefix, "/vdevs/vdev%d", v);
  h->m_enabled = true;
  h->m_id = 0xff;
  h->m_lun = 0xff;
  m_vdevcnt++;
}

void mountcmd(int argc, char **argv) {
  char tmp_path[MAX_FILE_PATH+1];
  char *vdev_name = NULL;
  VirtualDevice_t *h;
  uint8_t v = 0xff;
  
  if(!strncmp("/vdevs/vdev", cmd_prefix, 11)) {
    vdev_name = cmd_prefix + 11;
    v = strtol(vdev_name, NULL, 0);
  }

  if(v < NUM_VDEV) {
    h = &m_vdev[v];
    if(h->m_blocksize == 0) {
      if(h->m_type == DEV_TAPE) {
        h->m_blocksize = 1024;
      } else {
        h->m_blocksize = 512;
      }
    }
  } else if(argc>=2) {
    errorlevel = -1;
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.print(": The mount command is not valid in this context.\r\n");
    return;
  } else {
    // TODO: Print out all mounted images
    return;
  }

  if(argc<2) {
    // TODO: Print out the mounted image
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.print(": No backingstore specified.\r\n");
    return;
  }

  fixupPath(tmp_path, argv[1]);

  if(!strncmp(tmp_path, "/sd/", 4)) {
    if(OpenDiskImage(h, tmp_path, 512)) {
      strcpy(h->m_filename, tmp_path);
      h->m_enabled = true;
      
      return;
    }
  }

  h->m_fileSize = 0;
  h->m_blocksize = 0;
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": Unsupported backingstore path.\r\n");
  return;
}


void mapcmd(int argc, char **argv) {
  char tmp_path[MAX_FILE_PATH+1];
  char *vdev_name = NULL;
  char *tgt_name = NULL;
  char *lun_name = NULL;
  char *suffix = NULL;
  VirtualDevice_t *h;
  uint8_t v = 0xff;
  uint8_t id = 0xff;
  uint8_t lun = 0xff;
  
  if(!strncmp("/vdevs/vdev", cmd_prefix, 11)) {
    vdev_name = cmd_prefix + 11;
    v = strtol(vdev_name, &suffix, 0);

    if((v < NUM_VDEV) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
      h = &m_vdev[v];
      if(argc>=2) {
        fixupPath(tmp_path, argv[1]);

        if(!strncmp("/tgts/tgt", tmp_path, 9)) {
          tgt_name = tmp_path + 9;
          id = strtol(tgt_name, &lun_name, 0);
          if((id < NUM_SCSIID) && (lun_name)) {
            if(!strncmp("/lun", lun_name, 4)) {
              lun_name += 4;
              lun = strtol(lun_name, &suffix, 0);
              if((lun < NUM_SCSILUN) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
                h->m_id = id;
                h->m_lun = lun;
                m_vdevmap[id][lun] = v;
                return;
              }
            }
          }
        }
        goto e_invalidtarget;
      } else {
        sprintf(tmp_left, "/vdevs/vdev%d/$mapping ", v);
        sprintf(tmp_right, " [/tgts/tgt%d/lun%d]\r\n", h->m_id, h->m_lun);
        printDirectory(0, 0, tmp_left, tmp_right);
        return;
      }
    }
  }
  if(!strncmp("/tgts/tgt", cmd_prefix, 9)) {
    tgt_name = cmd_prefix + 9;
    id = strtol(tgt_name, &lun_name, 0);
    if((id < NUM_SCSIID) && (lun_name)) {
      if(!strncmp("/lun", lun_name, 4)) {
        lun_name += 4;
        lun = strtol(lun_name, &suffix, 0);
        if((lun < NUM_SCSILUN) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
          if(argc>=2) {
            fixupPath(tmp_path, argv[1]);
            
            if(!strncmp("/vdevs/vdev", tmp_path, 11)) {
              vdev_name = cmd_prefix + 11;
              v = strtol(vdev_name, &suffix, 0);
          
              if((v < NUM_VDEV) && (!suffix || (suffix[0] == 0) || !strcmp(suffix, "/"))) {
                h = &m_vdev[v];
                h->m_id = id;
                h->m_lun = lun;
                m_vdevmap[id][lun] = v;
                return;
              }
            }
            goto e_invalidtarget;
          } else {
            v = m_vdevmap[id][lun];
            if(v < NUM_VDEV) {
              sprintf(tmp_left, "/tgts/tgt%d/lun%d/$mapping ", id, lun);
              sprintf(tmp_right, " [/vdevs/vdev%d]\r\n", v);
              printDirectory(0, 0, tmp_left, tmp_right);
            }
            return;
          }
        }
      }
    }
  }

//e_invalidcontext:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.print(": The map command is not valid in this context.\r\n");
  return;

e_invalidtarget:
  errorlevel = -1;
  Serial.print("ERROR");
  if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
  Serial.printf(": Invalid target '%s' specified.\r\n", tmp_path);
  return;
}

void saveconfig(int argc, char **argv) {
  FsFile config_file;

  config_file = sd.open("/greenscsi.cfg", O_WRONLY | O_CREAT | O_TRUNC);
  if(!config_file.isOpen()) {
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Unable to open 'greenscsi.cfg' for writing.\r\n");
    return;
  }
  config_file.print( "@echo off\r\n\r\n");

  if(scsi_id_mask != 0) {
    config_file.print( "cd /tgts\r\n");
    for(uint8_t id = 0; id < NUM_SCSIID; id++) {
      if(scsi_id_mask & (1<<id)) {
        config_file.printf("enable tgt%d\r\n", id);
      }
    }
  }
  config_file.print( "\r\n");

  config_file.print( "cd /vdevs\r\n");
  for(uint8_t id = 0; id < NUM_SCSIID; id++) {
    for(uint8_t lun = 0; lun < NUM_SCSILUN; lun++) {
      uint8_t v = m_vdevmap[id][lun];
      if(v < NUM_VDEV) {
        VirtualDevice_t *h = &m_vdev[v];
        if(h->m_enabled) {
          // The create command automatically changes directory to the created object
          switch(h->m_type) {
            case DEV_DISK:
              config_file.print("create disk\r\n");
              break;
            case DEV_OPTICAL:
              config_file.print("create optical\r\n");
              break;
            case DEV_TAPE:
              config_file.print("create tape\r\n");
              break;
            default:
              config_file.printf("create 0x%02x\r\n", h->m_type);
              break;
          }
          if(h->m_file) {
            config_file.printf("mount %s\r\n", h->m_filename);
          }
          if(h->m_blocksize) {
            config_file.printf("set blocksize %d\r\n", h->m_blocksize);
          }
    
          // The map command should be last, as it will escape back to /vdevs
          if((h->m_id < NUM_SCSIID) && (h->m_lun < NUM_SCSILUN)) {
            config_file.printf("map /tgts/tgt%d/lun%d\r\n", h->m_id, h->m_lun);
          } else {
            config_file.print("cd ..\r\n");
          }
          config_file.print( "\r\n");
        }
      }
    }
  }
  config_file.close();
}

char helponSave[] =
  "\r\n"
  "saveconfig\r\n"
  "\r\n"
  "  Generate a new greenscsi.cfg file containing currently configured parameters.\r\n"
  "Any existing greenscsi.cfg file will be overwritten and its previous contents lost.\r\n"
  "\r\n"
;

char helponSet[] =
  "\r\n"
  "set <key> <value>\r\n"
  "\r\n"
  "  Use the set command to configure vdev parameters.\r\n"
  "\r\n"
;

char helponCreate[] =
  "\r\n"
  "create <type>\r\n\r\n"
  "  The create command allocates and changes the path to a newly created vdev.\r\n"
  "Recognized values for type:"
#if SUPPORT_DISK
  " disk"
#endif
#if SUPPORT_OPTICAL
  " optical"
#endif
#if SUPPORT_TAPE
  " tape"
#endif
  ".\r\n"
  "You can also specify the SCSI device code value directly.\r\n"
  "\r\n"
;

char helponEcho[] =
  "\r\n"
  "echo [on|off|<message>]\r\n"
  "\r\n"
  "  The echo command allows you to turn on or off the echoing of script commands to the console.\r\n"
  "Additionally, you can use the echo command to relay a message to the console.\r\n"
  "\r\n"
;

char helponCat[] =
  "\r\n"
  "cat <file>\r\n"
  "\r\n"
  "  The cat command dumps files to the console for reading.\r\n"
  "\r\n"
;

char helponUnlink[] =
  "\r\n"
  "unlink <path>\r\n"
  "\r\n"
  "  The unlink command removes objects such as files or vdevs.\r\n"
  "\r\n"
;

char helponExec[] =
  "\r\n"
  "exec <script>\r\n"
  "\r\n"
  "  The exec command executes command script files.\r\n"
  "At startup, greenscsi.cfg will be executed from the sd card if found.\r\n"
  "\r\n"
;

char helponEnable[] =
  "\r\n"
  "enable tgt<n>\r\n"
  "\r\n"
  "  The enable command sets a SCSI device ID to be handled by GreenSCSI.\r\n"
  "\r\n"
;

char helponMapT[] =
  "\r\n"
  "map <vdev>\r\n"
  "\r\n"
  "  The map command associates a given vdev with this lun.\r\n"
  "\r\n"
;

char helponMapV[] =
  "\r\n"
  "map <lun>\r\n"
  "\r\n"
  "  The map command associates a given lun with this vdev.\r\n"
  "\r\n"
;

char helponMount[] =
  "\r\n"
  "mount <path>\r\n"
  "\r\n"
  "  The mount command associates a given image or block device with this vdev.\r\n"
  "\r\n"
;

char helponVar[] =
  "\r\n"
  "setvar <key> <value>\r\n"
  "unsetvar <key>\r\n"
  "getvar <key>\r\n"
  "\r\n"
  "  The var commands allow you to perform rudementary environment variable manipulation.\r\n"
  "\r\n"
;

char helponMkImg[] =
  "\r\n"
  "mkimg <file> <type> <size>\r\n"
  "\r\n"
  "  The mkimg command creates image files on ExFat volumes.\r\n"
  "\r\n"
  "<type> specifies the partitioning scheme to preload the image with.\r\n"
  "       generic creates a blank disk ready for partitioning.\r\n"
  "       msdos preloads an MBR boot menu, and in the future may pre-partition and format.\r\n"
  "\r\n"
  "<size> specifies file size and supports a KB,MB,GB (1000) or KiB,MiB,GiB (1024) suffix.\r\n"
  "\r\n"
;

char helponHelp[] =
  "\r\n"
  "help\r\n"
  "\r\n"
  "  This help facility will show you the commands available within the current path.\r\n"
  "Some commands may have longer help articles such as this one to describe their parameters fully.\r\n"
  "\r\n"
;

Commands_t GlobalCommands[] = {
  // Command      Valid From Path   Req. Params    Short Help                    Long Help       Handler           Dispatch
  { "cd",         "/",              1,             "change current directory",   NULL,           changeDirectory,  NULL },
  { "sl",         "/",              0,             NULL,                         NULL,           slcmd,            NULL },
  { "dir",        "/",              0,             NULL,                         NULL,           punishDirectory,  NULL },
  { "ls",         "/",              0,             "display directory contents", NULL,           showDirectory,    NULL },
  { "saveconfig", "/",              0,             "write greenscsi.cfg file",   helponSave,     saveconfig,       NULL },
  { "enable",     "/tgts",          1,             "<target>",                   helponEnable,   enablecmd,        NULL },
  { "map",        "/tgts/tgt",      1,             "<vdev>",                     helponMapT,     mapcmd,           NULL },
  { "create",     "/vdevs",         1,             "<type>",                     helponCreate,   createcmd,        NULL },
  { "set",        "/vdevs/vdev",    2,             "<key> <value>",              helponSet,      setcmd,           NULL },
  { "mount",      "/vdevs/vdev",    1,             "<path>",                     helponMount,    mountcmd,         NULL },
  { "map",        "/vdevs/vdev",    1,             "<lun>",                      helponMapV,     mapcmd,           NULL },
  { "cat",        "/sd",            1,             "<file>",                     helponCat,      catcmd,           NULL },
  { "mkimg",      "/sd",            1,             "<file>",                     helponMkImg,    makeimagecmd,     NULL },
  { "unlink",     "/",              1,             "<path>",                     helponUnlink,   unlinkcmd,        NULL },
  { "exec",       "/",              1,             "<script>",                   helponExec,     execcmd,          NULL },
  { "goto",       "/",              1,             NULL,                         NULL,           gotocmd,          NULL },
  { "exit",       "/",              0,             NULL,                         NULL,           exitcmd,          NULL },
  { "setvar",     "/",              2,             "<key> <value>",              helponVar,      setvarcmd,        NULL },
  { "getvar",     "/",              0,             "[key]",                      helponVar,      getvarcmd,        NULL },
  { "unsetvar",   "/",              1,             "<key>",                      helponVar,      unsetvarcmd,      NULL },
  { "echo",       "/",              0,             "[on|off|<message>]",         helponEcho,     echocmd,          NULL },
  { "clear",      "/",              0,             "Erase the screen",           NULL,           clearcmd,         NULL },
  { "help",       "/",              0,             "This help facility.",        helponHelp,     help,             NULL },
  { NULL,         NULL,             0,             NULL,                         NULL,           NULL,             NULL }
};
