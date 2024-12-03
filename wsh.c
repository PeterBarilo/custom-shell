/* ************************************************************************
> File Name:     letter-boxed.c
> Author:        Chengtao Dai
> cs login:         chengtao
> Created Time:  Tue  10/1 12:31:17 2024
> Description:  See README.md

 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <ctype.h>
#include "wsh.h"

// error message for any kind of invalid operation
const char *error_message = "An error has occurred\n";
ShellVar shell_vars[MAX_VARS];
int num_vars = 0;
history_t history = {NULL, DEFAULT_HISTORY_SIZE, 0, 0, 0};
int last_cmd_rc = 0;

void init_path()
{
    setenv("PATH", "/bin", 1);
}

void interactive_mode()
{
    char command[MAX_COMMAND_LENGTH];

    while (1)
    {
        printf("wsh> ");
        fflush(stdout);

        // get user input
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL)
        {
            // fgets returns NULL on EOF, break the loop to exit
            break;
        }
        // remove the newline character from the command
        command[strcspn(command, "\n")] = 0;
        // ignore lines that are comments or empty
        if (is_comment(command) || strlen(command) == 0)
        {
            continue;
        }
        // check if the command is 'exit'
        if (strcmp(command, "exit") == 0)
        {
            break;
        }

        // execute the command
        execute_command(command);
    }
}

void batch_mode(const char *batch_file)
{
    FILE *file = fopen(batch_file, "r");
    if (file == NULL)
    {
        print_error("Error opening batch file");
        exit(1);
    }

    char command[MAX_COMMAND_LENGTH];
    while (fgets(command, MAX_COMMAND_LENGTH, file) != NULL)
    {
        // remove the newline character from the command
        command[strcspn(command, "\n")] = 0;
        // ignore lines that are comments or empty
        if (is_comment(command) || strlen(command) == 0)
        {
            continue;
        }
        // check if the command is 'exit'
        if (strcmp(command, "exit") == 0)
        {
            break;
        }

        // execute the command
        execute_command(command);
    }

    fclose(file);
}

// 1: > | 2: < | 3: >> | 4: &> | 5: &>> | 6. 2>
void handle_redirection(int redirect_type, char *filename)
{
    int fd;
    if (redirect_type == 1) // output redirection
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("Failed to open output file");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO); // redirect stdout to file
        close(fd);
    }
    else if (redirect_type == 2) // input redirection
    {
        fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            perror("Failed to open input file");
            exit(1);
        }
        dup2(fd, STDIN_FILENO); // redirect stdin to file
        close(fd);
    }
    else if (redirect_type == 3) // append output redirection
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1)
        {
            perror("Failed to open output file");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO); // redirect stdout to file
        close(fd);
    }
    else if (redirect_type == 4) // redirect stdout and stderr to the same file
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("Failed to open output file");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO); // redirect stdout
        dup2(fd, STDERR_FILENO); // redirect stderr
        close(fd);
    }
    else if (redirect_type == 5) // append stdout and stderr to the same file
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1)
        {
            perror("Failed to open output file");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO); // redirect stdout
        dup2(fd, STDERR_FILENO); // redirect stderr
        close(fd);
    }
    else if (redirect_type == 6) // redirect stderr only
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("Failed to open error file");
            exit(1);
        }
        dup2(fd, STDERR_FILENO); // redirect stderr to file
        close(fd);
    }
}

// parse redirection into a command string and set up redirect_type
void parse_redirection(char *command, int *redirect_type, char **filename)
{
    // check for redirection symbols in the command
    char *redir_pos = NULL;

    if (strstr(command, "2>"))
    {
        *redirect_type = 6; // redirect stderr only
        *filename = strstr(command, "2>") + 2; // filename follows 2>
        *strstr(command, "2>") = '\0'; // truncate command
    }
    else if ((redir_pos = strstr(command, ">")))
    {
        *redirect_type = 1; // output redirection
        *filename = redir_pos + 1; // filename follows >
        *redir_pos = '\0'; 
    }
    else if ((redir_pos = strstr(command, "<")))
    {
        *redirect_type = 2; // input redirection
        *filename = redir_pos + 1; 
        *redir_pos = '\0';
    }
    else if ((redir_pos = strstr(command, ">>")))
    {
        *redirect_type = 3; // append output redirection
        *filename = redir_pos + 2;
        *redir_pos = '\0'; 
    }
    else if ((redir_pos = strstr(command, "&>")))
    {
        *redirect_type = 4; // redirect both stdout and stderr
        *filename = redir_pos + 2; 
        *redir_pos = '\0';
    }
    else if ((redir_pos = strstr(command, "&>>")))
    {
        *redirect_type = 5; // append both stdout and stderr
        *filename = redir_pos + 3; 
        *redir_pos = '\0'; 
    }

    // remove leading spaces
    if (filename && *filename)
    {
        while (**filename == ' ')
        {
            (*filename)++;
        }
    }
}

// getter for a shell variable value
const char *get_shell_var(const char *varname)
{
    for (int i = 0; i < num_vars; i++)
    {
        if (strcmp(shell_vars[i].name, varname) == 0)
        {
            return shell_vars[i].value;
        }
    }
    return ""; // if variable doesn't exist, return empty string
}

// setter for a shell variable
void set_shell_var(const char *varname, const char *value)
{
    for (int i = 0; i < num_vars; i++)
    {
        if (strcmp(shell_vars[i].name, varname) == 0)
        {
            // update existing variable
            strcpy(shell_vars[i].value, value);
            return;
        }
    }
    // add new variable
    if (num_vars < MAX_VARS)
    {
        strcpy(shell_vars[num_vars].name, varname);
        strcpy(shell_vars[num_vars].value, value);
        num_vars++;
    }
    else
    {
        fprintf(stderr, "Error: Maximum number of shell variables reached.\n");
    }
}

// expand var after $ to value
void expand_variables(char *command)
{
    char expanded_command[MAX_COMMAND_LENGTH];
    char *read_ptr = command;
    char *write_ptr = expanded_command;
    while (*read_ptr)
    {
        if (*read_ptr == '$')
        {
            read_ptr++;
            char varname[MAX_VAR_LENGTH];
            char *var_ptr = varname;

            // collect valid variable name characters
            if (isalpha(*read_ptr) || *read_ptr == '_')
            {
                while (*read_ptr && (isalnum(*read_ptr) || *read_ptr == '_'))
                {
                    *var_ptr++ = *read_ptr++;
                }
                *var_ptr = '\0';

                // lookup variable value
                const char *var_value = getenv(varname);
                if (var_value == NULL)
                {
                    var_value = get_shell_var(varname);
                }

                // replace with variable value
                if (var_value)
                {
                    while (*var_value)
                    {
                        *write_ptr++ = *var_value++;
                    }
                }
            }
            else
            {
                // if not a valid variable name, treat '$' as a literal
                *write_ptr++ = '$';
            }
        }
        else
        {
            *write_ptr++ = *read_ptr++;
        }
    }
    *write_ptr = '\0';
    strcpy(command, expanded_command); // replace the original command
}

// local command handler for setting shell variables
void handle_local_command(char *command)
{
    // find the '=' character to split the variable name and value
    char *equal_sign = strchr(command, '=');

    if (equal_sign == NULL)
    {
        // no '=' found, invalid assignment
        fprintf(stderr, "Error: Invalid local variable assignment\n");
        last_cmd_rc = 1;
        return;
    }

    // null-terminate at '=' to extract the variable name
    *equal_sign = '\0';
    char *varname = command;
    char *value = equal_sign + 1;

    // check if the variable name starts with a '$'
    if (varname[0] == '$')
    {
        fprintf(stderr, "Error: Invalid variable name starting with $\n");
        last_cmd_rc = 1;
        return;
    }

    // check if the variable name contains only alphanumeric characters and underscores
    for (int i = 0; varname[i] != '\0'; i++)
    {
        if (!isalnum(varname[i]) && varname[i] != '_')
        {
            fprintf(stderr, "Error: Invalid variable name\n");
            last_cmd_rc = 1;
            return;
        }
    }

    // if variable name is valid
    if (varname != NULL && value != NULL)
    {
        // expand variables in value (e.g., local a=$b)
        expand_variables(value);
        set_shell_var(varname, value);
        last_cmd_rc = 0;
    }
    else if (varname != NULL && value == NULL)
    {
        set_shell_var(varname, ""); // clear the variable if no value is given
        last_cmd_rc = 1;
    }
}

// handle export command
void handle_export_command(char *command)
{
    char *varname = strtok(command, "=");
    char *value = strtok(NULL, "=");

    // if variable name is valid
    if (varname != NULL && value != NULL)
    {
        // set environment variable
        setenv(varname, value, 1); // 1 means overwrite existing value
        last_cmd_rc = 0;
    }
    else if (varname != NULL && value == NULL)
    {
        fprintf(stderr, "Error: export without value is not allowed\n");
        last_cmd_rc = 1;
    }
}

// handle the vars command (display shell variables)
void handle_vars_command()
{
    for (int i = 0; i < num_vars; i++)
    {
        printf("%s=%s\n", shell_vars[i].name, shell_vars[i].value);
    }
    last_cmd_rc = 0;
}

void init_history()
{
    history.commands = (char **)malloc(DEFAULT_HISTORY_SIZE * sizeof(char *));
    history.capacity = DEFAULT_HISTORY_SIZE;
}

// insert command to history
void insert_history(const char *command)
{
    // don't store built-in commands in history
    if (is_builtin_command(command))
    {
        return; 
    }

    // prevent consecutive duplicate commands
    if (history.count > 0)
    {
        int last_index = (history.end - 1 + history.capacity) % history.capacity;
        if (strcmp(history.commands[last_index], command) == 0)
        {
            return;
        }
    }

    // if history is full, overwrite the oldest command
    if (history.count == history.capacity)
    {
        free(history.commands[history.start]);
        history.commands[history.start] = strdup(command);
        history.start = (history.start + 1) % history.capacity;
        history.end = (history.end + 1) % history.capacity;
    }
    else
    {
        // add the new command to the end of the circular buffer
        history.commands[history.end] = strdup(command);
        history.end = (history.end + 1) % history.capacity;
        history.count++;
    }
}

void print_history()
{
    int index = history.start;
    for (int i = 0; i < history.count; i++)
    {
        printf("%d) %s\n", i + 1, history.commands[index]);
        index = (index + 1) % history.capacity;
    }
    last_cmd_rc = 0;
}

// execute the n-th command from history
void handle_history_command(int n)
{
    if (n <= 0 || n > history.count)
    {
        last_cmd_rc = 1;
        return; // Invalid command number
    }

    int index = (history.start + n - 1) % history.capacity;

    // Execute the command without adding it to history
    execute_command(history.commands[index]);
}

// set history to a given size
void set_history_size(int new_size)
{
    if (new_size <= 0)
    {
        last_cmd_rc = 1;
        return; 
    }

    // allocate new history buffer
    char **new_commands = (char **)malloc(new_size * sizeof(char *));
    int new_count = 0;
    int new_end = 0;

    // copy over commands that will fit into the new buffer
    int index = history.start;
    for (int i = 0; i < history.count && new_count < new_size; i++)
    {
        new_commands[new_end++] = history.commands[index];
        new_count++;
        index = (index + 1) % history.capacity;
    }

    // free the remaining commands that don't fit
    for (int i = new_count; i < history.count; i++)
    {
        free(history.commands[index]);
        index = (index + 1) % history.capacity;
    }

    // free the old history
    free(history.commands);

    // update history struct with new buffer
    history.commands = new_commands;
    history.capacity = new_size;
    history.count = new_count;
    history.start = 0;
    history.end = new_end;
    last_cmd_rc = 0;
}

// built-in cd command handler
void handle_cd_command(char *args[])
{
    if (args[1] == NULL || args[2] != NULL)
    {
        fprintf(stderr, "cd: wrong number of arguments\n");
        last_cmd_rc = 1;
        return;
    }
    if (chdir(args[1]) != 0)
    {
        perror("cd failed");
        last_cmd_rc = 1;
    }
    else
    {
        last_cmd_rc = 0;
    }
}

// helper compare function for qsort (alphabetical order)
int compare_filenames(const void *a, const void *b)
{
    const char *file_a = *(const char **)a;
    const char *file_b = *(const char **)b;
    return strcmp(file_a, file_b);
}

void handle_ls_command()
{
    DIR *dir;
    struct dirent *entry;
    char *filenames[1024]; // array to store file names, size will adjust later
    int count = 0;

    dir = opendir(".");
    if (!dir)
    {
        perror("ls");
        last_cmd_rc = 1;
        return;
    }

    // collect filenames from the directory
    while ((entry = readdir(dir)) != NULL)
    {
        // ignore hidden files (starting with .)
        if (entry->d_name[0] != '.')
        {                                             
            filenames[count] = strdup(entry->d_name); // duplicate the name for sorting
            count++;
        }
    }
    closedir(dir);

    // sort the filenames alphabetically
    qsort(filenames, count, sizeof(char *), compare_filenames);

    // print the sorted filenames
    for (int i = 0; i < count; i++)
    {
        printf("%s\n", filenames[i]);
    }
    last_cmd_rc = 0;
    // remember to free
    for (int i = 0; i < count; i++)
    {
        free(filenames[i]); 
    }
}

// print a specific error message to stderr
void print_error(const char *message)
{
    fprintf(stderr, "%s\n", message);
}

// helper function to check if a line starts with # 
int is_comment(char *line)
{
    // remove leading spaces
    while (*line == ' ')
    {
        line++;
    }
    // check if the line starts with '#'
    return (*line == '#');
}

// helper function to search for the executable in directories listed in PATH
int find_command_in_path(const char *command, char *full_path)
{
    char *path_env = getenv("PATH");
    char *path = strdup(path_env); // duplicate the PATH string for manipulation
    char *saveptr; // for strtok_r
    char *dir = strtok_r(path, ":", &saveptr); // split PATH by :

    // try each directory in PATH
    while (dir != NULL)
    {
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0)
        {
            free(path);
            return 1; // found the command and it's executable
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path);
    return 0; // command not found in any PATH directory
}

// check if a command is a built-in command
int is_builtin_command(const char *cmd)
{
    // extract the first token to compare
    char cmd_copy[MAX_COMMAND_LENGTH];
    snprintf(cmd_copy, sizeof(cmd_copy), "%s", cmd);
    char *token = strtok(cmd_copy, " ");

    if (token == NULL)
    {
        return 0;
    }

    return strcmp(token, "exit") == 0 || strcmp(token, "cd") == 0 || strcmp(token, "ls") == 0 ||
           strcmp(token, "local") == 0 || strcmp(token, "export") == 0 || strcmp(token, "vars") == 0 ||
           strcmp(token, "history") == 0;
}

// execute a command using execv 
void execute_command(char *command)
{
    // if not built-in, add command to history 
    if (!is_builtin_command(command))
    {
        insert_history(command);
    }

    char *args[MAX_ARGS];
    int i = 0;
    char *token;
    int redirect_type = 0;
    char *filename = NULL;

    // find the comment delimiter '#' 
    char *comment_pos = strchr(command, '#');
    if (comment_pos != NULL)
    {
        *comment_pos = '\0'; // truncate the command at that spot
    }

    // parse redirection operators and filenames
    parse_redirection(command, &redirect_type, &filename);

    // expand variables
    expand_variables(command);

    // tokenize the command into arguments
    token = strtok(command, " ");
    while (token != NULL && i < MAX_ARGS - 1)
    {
        args[i++] = token; // regular command/argument
        token = strtok(NULL, " ");
    }
    args[i] = NULL; // null-terminate the argument list

    // if no command, return without doing anything
    if (i == 0)
    {
        last_cmd_rc = 0;
        return;
    }

    // check if this is the exit command
    if (strcmp(args[0], "exit") == 0)
    {
        if (args[1] != NULL)
        {
            fprintf(stderr, "Error: exit does not take any arguments\n");
            last_cmd_rc = 255; 
        }
        else
        {
            exit(last_cmd_rc);
        }
        return;
    }

    // check if this is a local commadn
    if (strcmp(args[0], "local") == 0 && args[1] != NULL)
    {
        handle_local_command(args[1]); 
        return;
    }

    // check if this is an export command
    if (strcmp(args[0], "export") == 0 && args[1] != NULL)
    {
        handle_export_command(args[1]);
        return;
    }

    // check if this is the vars command
    if (strcmp(args[0], "vars") == 0)
    {
        handle_vars_command();
        return;
    }

    // check if this is a history command
    if (strcmp(args[0], "history") == 0)
    {
        if (args[1] == NULL)
        {
            print_history();
        }
        else if (strcmp(args[1], "set") == 0 && args[2] != NULL)
        {
            set_history_size(atoi(args[2]));
        }
        else
        {
            handle_history_command(atoi(args[1]));
        }
        return;
    }

    // check if this is a cd command
    if (strcmp(args[0], "cd") == 0)
    {
        handle_cd_command(args); 
        return;
    }

    // check if this is a ls command
    if (strcmp(args[0], "ls") == 0)
    {
        handle_ls_command(); 
        return;
    }

    // check if this is a full or relative path (contains /)
    if (strchr(args[0], '/') != NULL)
    {
        if (access(args[0], X_OK) == 0)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                print_error("Fork failed");
                last_cmd_rc = 1;
                return;
            }
            else if (pid == 0)
            {
                // child process: execute the command
                if (redirect_type > 0 && filename != NULL)
                {
                    handle_redirection(redirect_type, filename);
                }
                execv(args[0], args);
                // if execv returns, there was an error
                fprintf(stderr, "Command execution failed\n");
                exit(1);
            }
            else
            {
                // parent process: wait for the child to finish
                int status;
                waitpid(pid, &status, 0);

                if (WIFEXITED(status))
                {
                    last_cmd_rc = WEXITSTATUS(status);
                }
                else
                {
                    last_cmd_rc = 1;
                }
            }
        }
        else
        {
            last_cmd_rc = 255; // Command not found
        }
        return;
    }

    // construct the full path for the command
    char full_path[MAX_PATH_LENGTH];
    if (!find_command_in_path(args[0], full_path))
    {
        last_cmd_rc = 255; // Command not found
        return;
    }

    // Fork and execute the command
    pid_t pid = fork();
    if (pid < 0)
    {
        print_error("Fork failed");
        last_cmd_rc = 1;
        return;
    }
    else if (pid == 0)
    {
        // Child process: execute the command
        if (redirect_type > 0 && filename != NULL)
        {
            handle_redirection(redirect_type, filename);
        }
        execv(full_path, args);
        // If execv returns, there was an error
        fprintf(stderr, "Command execution failed\n");
        exit(1);
    }
    else
    {
        // Parent process: wait for the child to finish
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status))
        {
            last_cmd_rc = WEXITSTATUS(status);
        }
        else
        {
            last_cmd_rc = 1;
        }
    }
}

int main(int argc, char *argv[])
{
    init_path();
    init_history();

    if (argc > 2)
    {
        print_error("Usage: ./wsh [batch_file]");
        exit(1);
    }
    else if (argc == 2)
    {
        // Batch mode
        batch_mode(argv[1]);
    }
    else
    {
        // Interactive mode
        interactive_mode();
    }

    return last_cmd_rc;
}
