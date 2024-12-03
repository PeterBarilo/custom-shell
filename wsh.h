#ifndef WSH_H
#define WSH_H
#define MAX_VARS 100
#define MAX_LINE 1024   // Maximum command line length
#define MAX_ARGS 64     // Maximum number of arguments per command
#define MAX_VARS 100
#include <stdbool.h>

typedef struct {
    char name[MAX_LINE];
    char value[MAX_LINE];
} ShellVar;



void run_shell();                  // Main shell loop
void process_cmd(char *cmd, bool add_to_history);    // Execute a single command
int process_builtin(char **args);
void handle_redirection(char *cmd); // Handle redirection (>, <, etc.)
void history_add(char *cmd);
void show_history();                // Display the history
void cd(char *path);  // Built-in command to change directory
void handle_export(char *var) ;
void local(char *var);       // Built-in command to set shell variables
void handle_exit();                 
void ls();
char *get_var_value(const char *name);
void sub_var(char **args);
void show_vars();

extern int history_count;           
extern ShellVar shell_vars[MAX_VARS];
extern int var_count;

#endif
