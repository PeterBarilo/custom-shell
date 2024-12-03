#include "wsh.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <locale.h>
#include <sys/stat.h>

#define MAX_HISTORY_SIZE 100
#define DEFAULT_HISTORY_SIZE 5

int history_size = DEFAULT_HISTORY_SIZE;
char *history[MAX_HISTORY_SIZE];
int hist_count = 0; 
int last_exit_status = 0;  
ShellVar shell_vars[MAX_VARS];
int var_count = 0;
bool should_exit = false;
bool path_invalid = false;
int interactive_mode = 0;

// xtra funcs
bool is_builtin_command(char *cmd);  
char *trimmer(char *str);
int process_history_builtin(char **args);
int cmp_entries(const void *a, const void *b);

// infinte shell loop 
void shell_loop() {
    char line[MAX_LINE];

    while (1) {
        if (interactive_mode) {
            printf("wsh> ");
            fflush(stdout); 
        }

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (feof(stdin)) {
                break;
            }
            continue;
        }
        line[strcspn(line, "\n")] = '\0';
        char *trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t') {
            trimmed_line++;  
        }
        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') {
            continue;  
        }

        if (strcmp(trimmed_line, "exit") == 0) {
            break;
        }

        process_cmd(trimmed_line, true);
    }
}


void process_cmd(char *cmd, bool add_to_history) {
    char *args[MAX_ARGS];
    int i = 0;
    int saved_stdout = -1;
    int saved_stdin = -1;
    int  saved_stderr = -1;
    char *redirection_files[3] = {NULL, NULL, NULL}; 
    int redirection_types[3] = {0, 0, 0};            

    char original_cmd[MAX_LINE];
    strncpy(original_cmd, cmd, sizeof(original_cmd) - 1);
    original_cmd[sizeof(original_cmd) - 1] = '\0';

    if (add_to_history) {
        history_add(original_cmd);
    }

    char cmd_copy[MAX_LINE];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char *token = strtok(cmd_copy, " ");
    while (token != NULL && i < MAX_ARGS) {
        if (strcmp(token, "<") == 0 || strncmp(token, "<", 1) == 0 ||
            strcmp(token, ">") == 0 || strncmp(token, ">", 1) == 0 ||
            strcmp(token, ">>") == 0 || strncmp(token, ">>", 2) == 0 ||
            strncmp(token, "2>", 2) == 0 || strncmp(token, "1>", 2) == 0 || strncmp(token, "0>", 2) == 0) {
            char *redirection = token;
            char *file = NULL;

            // if with space
            if (strcmp(redirection, "<") == 0) {
                redirection_types[0] = 1;  
                token = strtok(NULL, " ");
                if (token == NULL) {
                    if (!interactive_mode) {
                        fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                    }
                    return;
                }
                redirection_files[0] = token;
            } else if (strncmp(redirection, "<", 1) == 0) {
                // if no space
                redirection_types[0] = 1;  
                file = redirection + 1;
                if (*file == '\0') {
                    if (!interactive_mode) {
                        fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                    }
                    return;
                }
                redirection_files[0] = file;
            } else if (strcmp(redirection, ">") == 0) {
                redirection_types[1] = 1;  
                token = strtok(NULL, " ");
                if (token == NULL) {
                    if (!interactive_mode) {
                        fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                    }
                    return;
                }
                redirection_files[1] = token;
            } else if (strncmp(redirection, ">", 1) == 0 && strncmp(redirection, ">>", 2) != 0) {
                redirection_types[1] = 1;  
                file = redirection + 1;
                if (*file == '\0') {
                    if (!interactive_mode) {
                        fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                    }
                    return;
                }
                redirection_files[1] = file;
            } else if (strcmp(redirection, ">>") == 0) {
                redirection_types[1] = 2;  
                token = strtok(NULL, " ");
                if (token == NULL) {
                    if (!interactive_mode) {
                        fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                    }
                    return;
                }
                redirection_files[1] = token;
            } else if (strncmp(redirection, ">>", 2) == 0) {
                // append no space
                redirection_types[1] = 2;  
                file = redirection + 2;
                if (*file == '\0') {
                    if (!interactive_mode) {
                        fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                    }
                    return;
                }
                redirection_files[1] = file;
            } else if (strncmp(redirection, "2>", 2) == 0) {
                // stderr 
                redirection_types[2] = 1;  
                file = redirection + 2;
                if (*file == '\0') {
                    token = strtok(NULL, " ");
                    if (token == NULL) {
                        if (!interactive_mode) {
                            fprintf(stderr, "wsh: syntax error near unexpected token `newline'\n");
                        }
                        return;
                    }
                    redirection_files[2] = token;
                } else {
                    redirection_files[2] = file;
                }
            }
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    sub_var(args);

    if (args[0] == NULL) {
        return;  
    }

    // input Redirection 
    if (redirection_files[0] != NULL) {
        int fd = open(redirection_files[0], O_RDONLY);
        if (fd < 0) {
            if (!interactive_mode) {
                perror("open");
            }
            return;
        }
        saved_stdin = dup(STDIN_FILENO);
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    // osutput redirection
    if (redirection_files[1] != NULL) {
        int fd;
        if (redirection_types[1] == 1) {  // Overwrite
            fd = open(redirection_files[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else {  // Append
            fd = open(redirection_files[1], O_WRONLY | O_CREAT | O_APPEND, 0644);
        }
        if (fd < 0) {
            if (!interactive_mode) {
                perror("open");
            }
            return;
        }
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    // stderr redirect
    if (redirection_files[2] != NULL) {
        int fd = open(redirection_files[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            if (!interactive_mode) {
                perror("open");
            }
            return;
        }
        saved_stderr = dup(STDERR_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    int builtin_status = process_builtin(args);
    if (builtin_status != -1) {
        last_exit_status = builtin_status;

        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stderr != -1) {
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }

        return;  
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (!interactive_mode) {
            perror("Fork failed");
        }
        last_exit_status = 127;  
        return;
    }

    if (pid == 0) {
        if (strchr(args[0], '/') != NULL) {
            execv(args[0], args);
        } else {
            char *path_env = getenv("PATH");
            if (path_env == NULL) {
                if (!interactive_mode) {
                    perror("getenv");
                }
                exit(127);
            }

            char path_copy[MAX_LINE];
            snprintf(path_copy, sizeof(path_copy), "%s", path_env);
            char *path = strtok(path_copy, ":");
            while (path != NULL) {
                char full_path[MAX_LINE];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, args[0]);

                if (access(full_path, X_OK) == 0) {
                    execv(full_path, args);
                }

                path = strtok(NULL, ":");
            }
        }

        if (!interactive_mode) {
            fprintf(stderr, "wsh: command not found: %s\n", args[0]);
        }
        exit(127);
    } else {
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        }
    }

    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stderr != -1) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
}


int process_builtin(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || args[2] != NULL) {
            if (!interactive_mode) {
                fprintf(stderr, "wsh: cd: wrong number of arguments\n");
            }
            return 1;
        } else {
            cd(args[1]);
        }
        return 0;
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            if (!interactive_mode) {
                perror("wsh: pwd");
            }
        }
        return 0;
    } else if (strcmp(args[0], "export") == 0) {
        handle_export(args[1]); 
        return 0;
    } else if (strcmp(args[0], "local") == 0) {
        local(args[1]);
        return 0;
    } else if (strcmp(args[0], "vars") == 0) {
        show_vars();
        return 0;
    } else if (strcmp(args[0], "history") == 0) {
        return process_history_builtin(args);
    } else if (strcmp(args[0], "ls") == 0) {
        ls();
        return 0;
    } else if (strcmp(args[0], "exit") == 0) {
        handle_exit();
        return 0;
    }
    return -1;
}


// helper for displaying local vars
void show_vars() {
    for (int i = 0; i < var_count; i++) {
        printf("%s=%s\n", shell_vars[i].name, shell_vars[i].value);
    }
}




int cmp_entries(const void *a, const void *b) {
    const char **entryA = (const char **)a;
    const char **entryB = (const char **)b;
    return strcmp(*entryA, *entryB);
}

void ls() {
    DIR *dir;
    struct dirent *entry;
    char *entries[MAX_LINE];
    int count = 0;

    setlocale(LC_COLLATE, "C");

    dir = opendir(".");
    if (dir == NULL) {
        perror("wsh: ls");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            entries[count++] = strdup(entry->d_name); 
        }
    }
    closedir(dir);
    qsort(entries, count, sizeof(char *), cmp_entries);

    for (int i = 0; i < count; i++) {
        printf("%s\n", entries[i]);
        free(entries[i]);
    }
}


void cd(char *path) {
    if (path == NULL) {
        fprintf(stderr, "wsh: cd: missing argument\n");
    } else if (chdir(path) != 0) {
        perror("wsh: cd");
    }
}

void handle_exit() {
    exit(EXIT_SUCCESS);
	}

void handle_export(char *var) {
    if (var == NULL) {
        if (!interactive_mode) {
            fprintf(stderr, "wsh: export: missing argument\n");
        }
        last_exit_status = 1;
        return;
    }

    char var_copy[MAX_LINE];
    snprintf(var_copy, sizeof(var_copy), "%s", var);

    char *name = strtok(var_copy, "=");
    char *value = strtok(NULL, "=");
    if (value == NULL) {
        if (!interactive_mode) {
            fprintf(stderr, "wsh: export: invalid argument\n");
        }
        last_exit_status = 1;
        return;
    }

    if (strcmp(name, "PATH") == 0) {
        char path_copy[MAX_LINE];
        snprintf(path_copy, sizeof(path_copy), "%s", value);
        char *path = strtok(path_copy, ":");
        bool all_invalid = true;  
        while (path != NULL) {
            struct stat statbuf;
            if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                all_invalid = false;
                break;  
            }
            path = strtok(NULL, ":");
        }
        if (all_invalid) {
            if (!interactive_mode) {
                fprintf(stderr, "wsh: export: invalid PATH value\n");
            }
            path_invalid = true;
        }
    }

    if (setenv(name, value, 1) != 0) {
        if (!interactive_mode) {
            perror("wsh: export");
        }
        last_exit_status = 1;
        return;
    }

    last_exit_status = 0;
}


void local(char *var) {
    if (var == NULL) {
        fprintf(stderr, "wsh: local: missing argument\n");
        return;
    }

    char *name = strtok(var, "=");
    char *value = strtok(NULL, "=");
    if (value == NULL) {
        value = "";  
    }

    // variable replacement
    if (value[0] == '$') {
        char *var_name = value + 1; 
        
        char *env_value = getenv(var_name);
        if (env_value != NULL) {
            value = env_value;  
        } else {
            for (int j = 0; j < var_count; j++) {
                if (strcmp(shell_vars[j].name, var_name) == 0) {
                    value = shell_vars[j].value; 
                    break;
                }
            }
        }
    }

    for (int i = 0; i < var_count; i++) {
        if (strcmp(shell_vars[i].name, name) == 0) {
            strcpy(shell_vars[i].value, value);
            return;
        }
    }

    if (var_count < MAX_VARS) 
    {
        strcpy(shell_vars[var_count].name, name);
        strcpy(shell_vars[var_count].value, value);
        var_count++;
    } else {
        fprintf(stderr, "wsh: local: too many variables\n");
    }
}


// substitition helper
void sub_var(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (args[i][0] == '$') {
            char *var_name = args[i] + 1;  
            char *env_value = getenv(var_name);
            if (env_value != NULL) {
                args[i] = env_value; 
                continue;
            }

            for (int j = 0; j < var_count; j++) {
                if (strcmp(shell_vars[j].name, var_name) == 0) {
                    args[i] = shell_vars[j].value; 
                    break;
                }
            }
        }
    }
}


void history_add(char *cmd) {
    if (history_size == 0) {
        return; 
    }

    char trimmed_cmd[MAX_LINE];
    strncpy(trimmed_cmd, cmd, sizeof(trimmed_cmd) - 1);
    trimmed_cmd[sizeof(trimmed_cmd) - 1] = '\0';

    char *start = trimmed_cmd;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    if (*start == '\0') {
        return;
    }
    if (is_builtin_command(start)) {
        return;  
    }
    if (hist_count > 0 && strcmp(start, history[hist_count - 1]) == 0) {
        return;
    }
    if (hist_count < history_size) {
        history[hist_count++] = strdup(start);
    } else {
        free(history[0]);
        for (int i = 1; i < history_size; i++) {
            history[i - 1] = history[i];
        }
        history[history_size - 1] = strdup(start);
    }
}



//helper to trim whitespace
char *trimmer(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    if (*str == 0) {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) {
        end--;
    }

    *(end + 1) = '\0';

    return str;
}



void show_hist() {
    for (int i = hist_count - 1; i >= 0; i--) {
        printf("%d) %s\n", hist_count - i, history[i]);
    }
}

// helper tp  check if command is builtin
bool is_builtin_command(char *cmd) {
    char temp_cmd[MAX_LINE];
    strncpy(temp_cmd, cmd, sizeof(temp_cmd) - 1);
    temp_cmd[sizeof(temp_cmd) - 1] = '\0';
    char *first_token = strtok(temp_cmd, " ");

    if (first_token == NULL) {
        return false;
    }

    if (strcmp(first_token, "cd") == 0 ||  
        strcmp(first_token, "export") == 0 || strcmp(first_token, "local") == 0 || 
        strcmp(first_token, "vars") == 0 || strcmp(first_token, "ls") == 0 || 
        strcmp(first_token, "exit") == 0 || strcmp(first_token, "history") == 0) {
        return true;
    }
    return false;
}


// get command from history
void process_history_command(int index) {
    if (index < 0 || index >= hist_count) {
        fprintf(stderr, "wsh: no such command in history\n");
        return;
    }

    char *command = history[index];
    printf("%s\n", command);  
    process_cmd(command, false); 
}

int process_history_builtin(char **args) {
    if (args[1] == NULL) {
        show_hist();  
        return 0;
    }

    if (strcmp(args[1], "set") == 0) {
        if (args[2] == NULL) {
            fprintf(stderr, "wsh: missing size for history set\n");
            return 1;
        }
        char *endptr;
        int new_size = strtol(args[2], &endptr, 10);
        if (*endptr != '\0' || new_size < 0 || new_size > MAX_HISTORY_SIZE) {
            fprintf(stderr, "wsh: invalid history size\n");
            return 1;
        }

        //clear if 0
        if (new_size == 0) {
            for (int i = 0; i < hist_count; i++) {
                free(history[i]);
                history[i] = NULL;
            }
            hist_count = 0;
        } else if (new_size < history_size) {
            int items_to_remove = hist_count - new_size;
            if (items_to_remove > 0) {
                for (int i = 0; i < items_to_remove; i++) {
                    free(history[i]);
                }
                for (int i = 0; i < hist_count - items_to_remove; i++) {
                    history[i] = history[i + items_to_remove];
                }
                hist_count -= items_to_remove;
            }
        }
        history_size = new_size;
        return 0;
    }

    char *endptr;
    int index = strtol(args[1], &endptr, 10);  
    if (*endptr != '\0' || index <= 0 || index > hist_count) {
        fprintf(stderr, "wsh: invalid history index\n");
        return 1;
    }
    index = hist_count - index;  
    process_history_command(index);

    return 0;
}



int main(int argc, char *argv[]) {
    if (setenv("PATH", "/bin", 1) != 0) {
        perror("Failed to set PATH");
        return 1;  
    }

    if (argc == 1) {
        interactive_mode = 1;  
        shell_loop();  
    } else if (argc == 2) {
        interactive_mode = 0;  
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            perror("Error opening batch file");
            exit(EXIT_FAILURE);
        }

        char line[MAX_LINE];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = '\0';

            char *trimmed_line = line;
            while (*trimmed_line == ' ' || *trimmed_line == '\t') {
                trimmed_line++;  
            }

            if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') {
                continue; 
            }

            process_cmd(trimmed_line, true);
        }
        fclose(file);
    } else {
        fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (path_invalid) {
        return 255;  //invalid path
    } else {
        return last_exit_status;  
    }
}
