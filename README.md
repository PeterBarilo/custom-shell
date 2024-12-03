# WSH - Custom Shell Program

WSH (Custom Shell Program) is a lightweight command-line interface implemented in C. It provides a minimal shell environment to execute basic commands and manage processes.

## Features
- **Command Execution**: Run built-in or system commands directly from the shell.
- **Process Management**: Handle background and foreground processes.
- **Interactive and Batch Mode**: Execute commands interactively or through a batch file.
- **Error Handling**: Provides informative error messages for invalid commands or improper usage.

## Compilation
To compile the program, ensure you have GCC installed and run the following command:

```bash
gcc wsh.c -Wall -Wextra -Werror -pedantic -std=gnu18 -o wsh
