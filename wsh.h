#ifndef WSH_H
#define WSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h> 

#define MAX_VARS 100  // maximum number of shell variables
#define MAX_VAR_LENGTH 100  // maximum length of a shell variable
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define DEFAULT_HISTORY_SIZE 5
#define MAX_PATH_LENGTH 1024

// struct to store shell variables
typedef struct {
    char name[MAX_VAR_LENGTH];
    char value[MAX_VAR_LENGTH];
} ShellVar;

// global variables and functions for handling shell variables
extern ShellVar shell_vars[MAX_VARS];
extern int num_vars;

// struct to handle history functionality
typedef struct {
    char **commands;
    int capacity;
    int count;
    int start;
    int end;
} history_t;

// global history object
extern history_t history;  
extern int last_command_status;  // Store the status of the last executed command

// init path
void init_path();

// shell modes
void interactive_mode();
void batch_mode(const char* batch_file);

// command execution
void execute_command(char* command);

// redirection
void handle_redirection(int redirect_type, char* filename);
void parse_redirection(char* command, int* redirect_type, char** filename); // set up redirection type

// env and shell variable handling
const char* get_shell_var(const char* varname);
void set_shell_var(const char* varname, const char* value);
void handle_local_command(char* command);
void expand_variables(char* command);
void handle_export_command(char* command);
void handle_vars_command();

// history-related functions
void init_history();
void insert_history(const char* cmd);
void print_history();  // Print the stored history
void handle_history_command(int n);
void set_history_size(int new_size);

// Built-in command handlers
// all built-in implemented
// 1. exit -> manually
// 2. cd 
// 3. export -> see handle_export_command
// 4. local -> see handle_local_command
// 5. vars -> see handle_vars_command
// 6. history -> see handle_history)command
// 7. ls
void handle_cd_command(char *args[]);
void handle_ls_command();

// helper functions
void print_error(const char* message);  // print error message to stderr
int is_comment(char* line);  // check if a line is a comment
int find_command_in_path(const char* command, char* full_path); // look for command at the given path
int is_builtin_command(const char* cmd);  // check if a command is a built-in command

#endif  // WSH_H
