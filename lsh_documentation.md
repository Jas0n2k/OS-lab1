# LSH Shell Documentation

## Overview
This is a simple shell implementation called `lsh` (Little Shell) that provides basic command execution, built-in commands, pipelines, input/output redirection, and background process support.

## Code Structure Analysis

### Headers and Includes
```c
#include <assert.h>        // Assertion macros for debugging
#include <ctype.h>         // Character classification functions
#include <stdio.h>         // Standard I/O functions
#include <stdlib.h>        // General utilities (malloc, exit, etc.)
#include <string.h>        // String manipulation functions
#include <readline/readline.h>  // GNU readline for command line editing
#include <readline/history.h>   // Command history functionality
#include <sys/types.h>     // System data types
#include <sys/wait.h>      // Process waiting functions
#include <unistd.h>        // POSIX operating system API
#include <errno.h>         // Error number definitions
#include <fcntl.h>         // File control operations
#include <signal.h>        // Signal handling
#include "parse.h"         // Custom parser for command parsing
```

### Global Function Declarations
```c
static void sigchld_handler(int sig);
static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
static int execute_cmd(Command *cmd);
void stripwhite(char *);
static int execute_pipeline(Pgm *p, int cmd_idx, int num_cmds, int *pipefds, Command *cmd);
```

## Core Functions

### 1. Signal Handler (`sigchld_handler`)
```c
static void sigchld_handler(int sig)
{
  // Reap all available zombie children
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}
```
**Purpose**: Prevents zombie processes by reaping terminated child processes.
- `waitpid(-1, NULL, WNOHANG)`: Wait for any child process (-1) without blocking (WNOHANG)
- Loop continues until no more zombie processes remain

### 2. Main Function (`main`)
```c
int main(void)
{
  signal(SIGINT, SIG_IGN);          // Ignore Ctrl-C in the shell
  signal(SIGCHLD, sigchld_handler); // Handle child process termination
```
**Setup Phase**:
- `signal(SIGINT, SIG_IGN)`: Makes shell ignore Ctrl+C (SIGINT signals)
- `signal(SIGCHLD, sigchld_handler)`: Sets up handler for child process termination

**Main Loop**:
```c
for (;;)
{
  // Reap any finished background processes
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  char *line;
  line = readline("lsh> ");
  if (line == NULL) // readline() returns NULL on EOF
  {
    printf("\nexit\n");
    break;
  }
```
- Continuously reaps background processes
- `readline("lsh> ")`: Displays prompt and reads user input with editing capabilities
- NULL return indicates EOF (Ctrl+D), causing shell to exit

**Command Processing**:
```c
stripwhite(line);
if (*line)
{
  add_history(line);
  Command cmd;
  if (parse(line, &cmd) == 1)
  {
    print_cmd(&cmd);
    execute_cmd(&cmd);
  }
  else
  {
    printf("Parse ERROR\n");
  }
}
free(line);
```
- `stripwhite(line)`: Removes leading/trailing whitespace
- `add_history(line)`: Adds command to readline history
- `parse(line, &cmd)`: Parses command line into Command structure
- `execute_cmd(&cmd)`: Executes the parsed command

### 3. Built-in Commands (`check_built_ins`)
```c
static int check_built_ins(Pgm *current_pgm, int argc)
{
  // Built-in command: cd
  if (strcmp(current_pgm->pgmlist[0], "cd") == 0)
  {
    if (argc > 2)
    {
      fprintf(stderr, "cd: too many arguments\n");
      return -1;
    }

    if (current_pgm->pgmlist[1] == NULL)
    {
      fprintf(stderr, "cd : missing argument");
    }
    else if (chdir(current_pgm->pgmlist[1]) != 0)
    {
      perror("cd");
    }
    return 0;
  }
```
**CD Command**:
- `strcmp()`: Compares command name with "cd"
- `chdir()`: Changes current working directory
- `perror()`: Prints system error message

**Exit Command**:
```c
else if (strcmp(current_pgm->pgmlist[0], "exit") == 0)
{
  if (argc > 1)
  {
    fprintf(stderr, "exit: too many arguments\n");
    return -1;
  }
  printf("\nexit\n");
  exit(0);
}
```
- `exit(0)`: Terminates the shell process

### 4. Command Execution (`execute_cmd`)
```c
static int execute_cmd(Command *cmd)
{
  char *cmd_name = cmd->pgm->pgmlist[0];
  int argc = 0;
  while (cmd->pgm->pgmlist[argc] != NULL)
    argc++;
```
**Initialization**:
- Counts arguments in the command
- Gets the command name

**Pipeline Setup**:
```c
// Count commands
int num_cmds = 0;
for (Pgm *tmp = current_pgm; tmp; tmp = tmp->next)
  num_cmds++;

// Create pipes
int pipefds[2 * (num_cmds - 1)];
for (int i = 0; i < num_cmds - 1; i++)
{
  if (pipe(pipefds + 2 * i) < 0)
  {
    perror("pipe");
    exit(1);
  }
}
```
- Counts commands in pipeline
- Creates `(num_cmds - 1)` pipes using `pipe()` system call
- Each pipe requires 2 file descriptors (read end, write end)

**Process Management**:
```c
execute_pipeline(current_pgm, 0, num_cmds, pipefds, cmd);

// Close all pipe fds in parent
for (int i = 0; i < 2 * (num_cmds - 1); i++)
{
  close(pipefds[i]);
}

// Wait for children
if (!cmd->background)
{
  for (int i = 0; i < num_cmds; i++)
    wait(NULL);
}
```
- Executes pipeline recursively
- Closes all pipe file descriptors in parent
- Waits for foreground processes using `wait()`

### 5. Pipeline Execution (`execute_pipeline`)
```c
static int execute_pipeline(Pgm *p, int cmd_idx, int num_cmds, int *pipefds, Command *cmd)
{
  if (p == NULL)
    return 0;

  // Recurse first to reach the earliest command
  execute_pipeline(p->next, cmd_idx + 1, num_cmds, pipefds, cmd);

  // Fork this command AFTER recursion unwinds
  pid_t pid = fork();
```
**Recursive Structure**:
- Processes commands in reverse order (last command first)
- `fork()`: Creates child process for each command

**Child Process Setup**:
```c
if (pid == 0)
{
  // only set default signal handler for SIGINT if not background
  if (!cmd->background)
    signal(SIGINT, SIG_DFL);
```
- Restores default SIGINT handling for foreground processes
- Background processes continue to ignore SIGINT

**Input Redirection**:
```c
// Handle input redirection (only for first command in pipeline)
if (cmd_idx == num_cmds - 1 && cmd->rstdin)
{
  int fd_in = open(cmd->rstdin, O_RDONLY);
  if (fd_in < 0)
  {
    perror("open input file");
    exit(1);
  }
  dup2(fd_in, STDIN_FILENO);
  close(fd_in);
}
// read previous pipe if not first in pipeline
else if (cmd_idx < num_cmds - 1)
{
  dup2(pipefds[2 * cmd_idx], STDIN_FILENO);
}
```
- `open()`: Opens input file for redirection
- `dup2()`: Duplicates file descriptor to stdin
- Connects pipe read end to stdin for pipeline commands

**Output Redirection**:
```c
// Handle output redirection (only for last command in pipeline)
if (cmd_idx == 0 && cmd->rstdout)
{
  int fd_out = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd_out < 0)
  {
    perror("open output file");
    exit(1);
  }
  dup2(fd_out, STDOUT_FILENO);
  close(fd_out);
}
// Not the last in pipeline → write to next pipe
else if (cmd_idx > 0)
{
  dup2(pipefds[2 * (cmd_idx - 1) + 1], STDOUT_FILENO);
}
```
- Creates/truncates output file with permissions 0644
- Connects pipe write end to stdout for pipeline commands

**Program Execution**:
```c
// Close all pipe fds in child
for (int i = 0; i < 2 * (num_cmds - 1); i++)
{
  close(pipefds[i]);
}

execvp(p->pgmlist[0], p->pgmlist);
perror("execvp");
exit(1);
```
- Closes all pipe file descriptors in child
- `execvp()`: Replaces process image with new program
- `perror()` only executes if `execvp()` fails

## External Functions Used

### System Calls
- **`fork()`**: Creates new process
- **`execvp()`**: Executes program, searching PATH
- **`wait()`/`waitpid()`**: Waits for child process termination
- **`pipe()`**: Creates pipe for inter-process communication
- **`dup2()`**: Duplicates file descriptor
- **`open()`**: Opens files
- **`close()`**: Closes file descriptors
- **`chdir()`**: Changes working directory
- **`signal()`**: Sets signal handlers

### Library Functions
- **`readline()`**: Advanced line input with editing
- **`add_history()`**: Adds command to history
- **`printf()`/`fprintf()`**: Formatted output
- **`perror()`**: Prints system error messages
- **`strcmp()`**: String comparison
- **`strlen()`**: String length
- **`memmove()`**: Memory copying
- **`isspace()`**: Character classification
- **`malloc()`/`free()`**: Memory management

### Custom Functions
- **`parse()`**: Parses command line into Command structure
- **`stripwhite()`**: Removes leading/trailing whitespace

## Key Features

1. **Interactive Command Line**: Uses GNU readline for advanced input editing
2. **Built-in Commands**: Implements `cd` and `exit` commands
3. **Pipeline Support**: Handles command pipelines with proper pipe setup
4. **I/O Redirection**: Supports input (`<`) and output (`>`) redirection
5. **Background Processes**: Executes commands in background with `&`
6. **Signal Handling**: Proper SIGINT and SIGCHLD handling
7. **Process Management**: Prevents zombie processes through proper reaping

## Process Flow

1. Shell starts and sets up signal handlers
2. Main loop displays prompt and reads command
3. Command is parsed into structured format
4. Built-in commands are checked and executed if found
5. For external commands, pipes are created for pipeline
6. Each command in pipeline is forked and executed
7. I/O redirection is set up in child processes
8. Parent waits for foreground processes or continues for background
9. Zombie processes are reaped continuously