# HINTS.md - OS Lab 1 Shell Implementation

This file provides implementation hints for each specification in the shell assignment.

## Understanding the Data Structures

### Command Structure
```c
typedef struct node {
    Pgm *pgm;          // Linked list of programs (for pipes)
    char *rstdin;      // Input redirection file
    char *rstdout;     // Output redirection file
    char *rstderr;     // Error redirection file (unused)
    int background;    // 1 if command should run in background
} Command;
```

### Program Structure
```c
typedef struct c {
    char **pgmlist;    // Array of strings (command + arguments)
    struct c *next;    // Next program in pipeline
} Pgm;
```

**Important**: The program list is in **reverse order**! The last command in the pipeline is first in the linked list.

## Command History (GNU Readline)

**Hints**:
- The skeleton code uses GNU readline library which provides automatic command history
- History functionality (Ctrl-P/Ctrl-N for previous/next commands) is built-in to `readline()`
- No additional implementation needed - readline handles this automatically
- History persists during the shell session
- You can optionally add history file persistence with `read_history()` and `write_history()`

**Optional History File Implementation**:
```c
#include <readline/history.h>

// At startup (optional)
read_history(".lsh_history");

// At exit (optional)
write_history(".lsh_history");
```

## 1. Ctrl-D Handling (EOF)

**Hint**:
- `readline()` returns `NULL` when Ctrl-D is pressed
- Check for this condition in your main loop
- Clean up resources and exit gracefully

```c
char *line = readline("> ");
if (line == NULL) {
    // Ctrl-D pressed
    printf("exit\n");  // Optional: print exit message
    exit(0);
}
```

## 2. Basic Commands

**Key System Calls**: `fork()`, `execvp()`, `wait()`

**Hints**:
- Use `fork()` to create child process
- In child: use `execvp()` to replace process image
- In parent: use `wait()` to wait for child completion
- `execvp()` searches PATH automatically
- First element of `pgmlist` is the command, rest are arguments
- `pgmlist` is already NULL-terminated

**Implementation Strategy**:
1. Fork a child process
2. In child: call `execvp(pgm->pgmlist[0], pgm->pgmlist)`
3. In parent: call `wait()` (unless background)

## 3. Background Execution

**Hints**:
- Check `cmd->background` flag
- If background: don't call `wait()` in parent
- Print process ID when starting background job
- Handle zombie cleanup with `waitpid()` and `WNOHANG`

**Implementation Strategy**:
```c
pid_t pid = fork();
if (pid == 0) {
    // Child process
    execvp(...);
} else if (pid > 0) {
    if (cmd->background) {
        printf("[%d]\n", pid);  // Print background job PID
        // Don't wait - return to prompt immediately
    } else {
        wait(NULL);  // Wait for foreground process
    }
}
```

## 4. Piping

**Key System Calls**: `pipe()`, `dup2()`, `close()`

**Hints**:
- Create pipes between each pair of programs
- Remember: program list is in **reverse order**
- For N programs, you need N-1 pipes
- Each child process needs to:
  - Close unused pipe ends
  - Redirect stdin/stdout to appropriate pipe ends
  - Close original pipe file descriptors after dup2

**Implementation Strategy**:
1. Count programs in the pipeline
2. Create array of pipes: `int pipes[n-1][2]`
3. For each program:
   - Fork child
   - Set up input (from previous pipe or stdin)
   - Set up output (to next pipe or stdout)
   - Close all pipe file descriptors
   - Execute program

**Pipe Setup Pattern**:
- First program: output goes to pipe[0][1]
- Middle programs: input from pipe[i-1][0], output to pipe[i][1]
- Last program: input comes from pipe[n-2][0]

## 5. I/O Redirection

**Key System Calls**: `open()`, `dup2()`, `close()`

**Hints**:
- Check `cmd->rstdin` and `cmd->rstdout` for redirection files
- Use `open()` with appropriate flags:
  - Input: `O_RDONLY`
  - Output: `O_WRONLY | O_CREAT | O_TRUNC`, mode `0644`
- Use `dup2()` to redirect file descriptors to stdin/stdout
- Handle redirection in child processes before `execvp()`

**Implementation**:
```c
if (cmd->rstdin) {
    int fd = open(cmd->rstdin, O_RDONLY);
    dup2(fd, STDIN_FILENO);
    close(fd);
}
if (cmd->rstdout) {
    int fd = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(fd, STDOUT_FILENO);
    close(fd);
}
```

## 6. Built-in Commands

**Hints**:
- Implement `cd` and `exit` as special cases before forking
- Check first argument of first program: `cmd->pgm->pgmlist[0]`
- For `cd`: use `chdir()` system call
- Handle `cd` with no arguments (go to home directory)
- For `exit`: clean up and call `exit(0)`

**Implementation**:
```c
char *command = cmd->pgm->pgmlist[0];
if (strcmp(command, "cd") == 0) {
    char *path = cmd->pgm->pgmlist[1];
    if (path == NULL) {
        path = getenv("HOME");  // Default to home directory
    }
    if (chdir(path) != 0) {
        perror("cd");
    }
    return;  // Don't fork for built-ins
}
if (strcmp(command, "exit") == 0) {
    exit(0);
}
```

## 7. Ctrl-C Handling (Signal Management)

**Key System Calls**: `signal()`, `kill()`, `getpid()`

**Hints**:
- Install signal handler for SIGINT in main process
- Foreground processes should receive SIGINT
- Background processes should be protected from SIGINT
- Use process groups or signal masking

**Implementation Strategy**:
```c
// In main function
signal(SIGINT, sigint_handler);

// Signal handler
void sigint_handler(int sig) {
    // Don't terminate shell, just return to prompt
    printf("\n");  // New line after ^C
}

// In child processes for foreground jobs
signal(SIGINT, SIG_DFL);  // Restore default behavior
```

## 8. No Zombies

**Key System Calls**: `waitpid()`, `WNOHANG`

**Hints**:
- Install SIGCHLD handler to reap background children
- Use `waitpid(-1, NULL, WNOHANG)` to reap zombies non-blockingly
- Call zombie cleanup regularly (e.g., before each prompt)

**Implementation**:
```c
// Install handler
signal(SIGCHLD, sigchld_handler);

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Keep reaping until no more zombies
    }
}

// Or call cleanup function before each prompt
void cleanup_zombies() {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap zombies
    }
}
```

## General Implementation Tips

### Error Handling
- Always check return values of system calls
- Use `perror()` to print meaningful error messages
- Handle edge cases (empty commands, non-existent files, etc.)

### File Descriptor Management
- Close all unnecessary file descriptors in child processes
- This is crucial for proper pipe operation
- Pattern: open, dup2, close original

### Process Management
- Keep track of child PIDs for proper cleanup
- Handle both foreground and background process waiting
- Use appropriate wait options (blocking vs non-blocking)

### Testing Strategy
1. Start with simple commands
2. Add redirection
3. Add background execution
4. Add piping
5. Test built-ins
6. Test signal handling
7. Test complex combinations

### Debugging Tips
- Use `ps aux | grep lsh` to check for zombies
- Use `lsof -p <pid>` to check open file descriptors
- Print debug information about pipes and file descriptors
- Test each feature incrementally

### Common Pitfalls
- Forgetting that program list is in reverse order
- Not closing all file descriptors in child processes
- Not handling EOF from readline
- Race conditions with signal handlers
- Memory leaks (use valgrind to check)

## Command Structure Example

For command `ls -l | wc > output.txt &`:
```
cmd->background = 1
cmd->rstdout = "output.txt"
cmd->pgm = first_pgm  (points to "wc")
    first_pgm->pgmlist = ["wc", NULL]
    first_pgm->next = second_pgm  (points to "ls -l")
        second_pgm->pgmlist = ["ls", "-l", NULL]
        second_pgm->next = NULL
```

Remember: Process the programs in reverse order of the linked list to get the correct pipeline order!

## Team Task Allocation (3 Members)

Since you have three team members and need to balance coding, report writing, and presentation duties, here's a suggested allocation:

### Primary Responsibility Distribution

**Member 1 - Core Execution Lead**
- **Coding**: Specifications 1, 2, 6 (Ctrl-D, Basic Commands, Built-ins)
- **Report**: Introduction, Basic Command Implementation, Built-in Commands sections
- **Presentation**: Demo basic functionality and built-in commands

**Member 2 - Advanced Features Lead** 
- **Coding**: Specifications 3, 4, 5 (Background, Piping, I/O Redirection)
- **Report**: Advanced Features sections (Background, Piping, Redirection), Testing methodology
- **Presentation**: Demo complex piping and redirection scenarios

**Member 3 - System Integration Lead**
- **Coding**: Specifications 7, 8 (Signal Handling, Zombie Prevention)
- **Report**: Signal Handling, Process Management, Challenges & Solutions, Conclusion
- **Presentation**: Demo signal handling, process monitoring, overall architecture

### Implementation Order & Collaboration

**Phase 1: Foundation (Week 1)**
- Member 1: Basic command execution framework
- Member 2: Study piping architecture, prepare I/O redirection design
- Member 3: Research signal handling, set up process monitoring tools

**Phase 2: Core Features (Week 2)**
- Member 1: Complete basic commands and built-ins
- Member 2: Implement I/O redirection first (easier than piping)
- Member 3: Implement background execution and basic signal handling

**Phase 3: Advanced Integration (Week 3)**
- Member 2: Implement piping (most complex, needs full focus)
- Member 1 & 3: Support piping implementation, handle edge cases
- All: Integration testing and debugging

**Phase 4: Polish & Documentation (Week 4)**
- All: Comprehensive testing with manual test cases
- Member 3: Final zombie cleanup and signal handling refinement  
- All: Report writing and presentation preparation

### Collaborative Guidelines

**Daily Standups**
- Share progress, blockers, and integration points
- Coordinate testing of interconnected features
- Plan next day's work

**Code Integration Strategy**
- Use feature branches for each specification
- Regular integration testing sessions
- Code reviews before merging

**Report Writing Coordination**
- Each member writes their primary sections
- Rotate for peer review of other sections
- Joint editing session for coherence and flow
- Aim for 450+ words per member (total 1350+ words)

**Testing Responsibility**
- Each member tests their own features thoroughly
- Cross-testing: test other members' features
- Integration testing sessions together
- Member 3 coordinates overall testing strategy

### Presentation Structure (15-20 minutes)

**Member 1** (5-7 min): Introduction & Basic Features
- Project overview and architecture
- Demo basic commands, error handling
- Built-in commands demonstration

**Member 2** (5-7 min): Advanced Features
- I/O redirection examples
- Complex piping scenarios
- Background process management

**Member 3** (5-6 min): System Design & Wrap-up
- Signal handling and Ctrl-C demo
- Process monitoring and zombie prevention
- Challenges overcome and lessons learned
- Q&A coordination

### Contingency Planning

**If Member Falls Behind**
- Redistribute less critical features
- Pair programming sessions for complex features
- Report sections can be redistributed more easily than code

**Feature Dependency Management**
- Background execution (Member 3) needs basic commands (Member 1) first
- Piping (Member 2) needs basic execution framework from Member 1
- Signal handling (Member 3) integrates with all features

**Quality Assurance**
- Each specification must pass all manual tests before integration
- No member commits code that breaks existing functionality
- Final integration testing is everyone's responsibility

This allocation ensures each member gets experience with different complexity levels while maintaining clear ownership and accountability.

## Key System Calls and Methods Reference

### Process Management

**fork()**
```c
#include <unistd.h>
pid_t fork(void);
```
- Creates child process identical to parent
- Returns: child PID in parent, 0 in child, -1 on error
- Use: Create separate process for command execution

**execvp()**
```c
#include <unistd.h>
int execvp(const char *file, char *const argv[]);
```
- Replaces current process image with new program
- Searches PATH for executable
- argv[0] should be program name, argv must be NULL-terminated
- Only returns on error (-1)

**wait() / waitpid()**
```c
#include <sys/wait.h>
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
```
- wait(): blocks until any child terminates
- waitpid(): more control, can be non-blocking with WNOHANG
- Use: Prevent zombie processes, synchronize with child completion

### File Operations

**open()**
```c
#include <fcntl.h>
int open(const char *pathname, int flags, mode_t mode);
```
- Flags: O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC
- Mode: 0644 for readable/writable files
- Returns: file descriptor or -1 on error

**close()**
```c
#include <unistd.h>
int close(int fd);
```
- Closes file descriptor
- Critical: close unused FDs in child processes

**dup2()**
```c
#include <unistd.h>
int dup2(int oldfd, int newfd);
```
- Duplicates oldfd to newfd
- Use: Redirect stdin/stdout to files or pipes
- Pattern: open file, dup2 to STDIN_FILENO/STDOUT_FILENO, close original

### Inter-Process Communication

**pipe()**
```c
#include <unistd.h>
int pipe(int pipefd[2]);
```
- Creates pipe with read end (pipefd[0]) and write end (pipefd[1])
- Data written to pipefd[1] can be read from pipefd[0]
- Use: Connect stdout of one process to stdin of another

### Signal Handling

**signal()**
```c
#include <signal.h>
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```
- Installs signal handler
- Common signals: SIGINT (Ctrl-C), SIGCHLD (child terminated)
- Handlers: SIG_DFL (default), SIG_IGN (ignore), custom function

**kill()**
```c
#include <sys/types.h>
#include <signal.h>
int kill(pid_t pid, int sig);
```
- Sends signal to process
- pid > 0: specific process, pid = 0: process group
- Use: Send SIGTERM/SIGKILL to terminate processes

### Directory Operations

**chdir()**
```c
#include <unistd.h>
int chdir(const char *path);
```
- Changes current working directory
- Returns 0 on success, -1 on error
- Use: Implement cd built-in command

**getcwd()**
```c
#include <unistd.h>
char *getcwd(char *buf, size_t size);
```
- Gets current working directory
- Use: Display current directory in prompt

### Environment Variables

**getenv()**
```c
#include <stdlib.h>
char *getenv(const char *name);
```
- Returns value of environment variable
- Use: Get HOME directory for cd without arguments
- Returns NULL if variable doesn't exist

### Memory Management

**malloc() / free()**
```c
#include <stdlib.h>
void *malloc(size_t size);
void free(void *ptr);
```
- Dynamic memory allocation
- Always free() what you malloc()
- readline() returns malloc'd string - must free()

### String Operations

**strcmp()**
```c
#include <string.h>
int strcmp(const char *s1, const char *s2);
```
- Compares strings
- Returns 0 if equal
- Use: Check for built-in commands

**strlen()**
```c
#include <string.h>
size_t strlen(const char *s);
```
- Returns string length
- Use: Validate command arguments

### Error Handling

**perror()**
```c
#include <stdio.h>
void perror(const char *s);
```
- Prints error message for last errno
- Use: Print meaningful error messages for system call failures

### Standard File Descriptors
```c
#include <unistd.h>
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
```

### Key Constants and Flags

**File Open Flags**
- `O_RDONLY`: Read only
- `O_WRONLY`: Write only  
- `O_CREAT`: Create if doesn't exist
- `O_TRUNC`: Truncate to zero length

**Wait Options**
- `WNOHANG`: Don't block if no child has exited
- `WUNTRACED`: Report stopped children

**Signals**
- `SIGINT`: Interrupt (Ctrl-C)
- `SIGCHLD`: Child process terminated
- `SIGTERM`: Termination request
- `SIGKILL`: Kill (cannot be caught)

### Common Usage Patterns

**Basic Command Execution**
```c
pid_t pid = fork();
if (pid == 0) {
    execvp(cmd->pgm->pgmlist[0], cmd->pgm->pgmlist);
    perror("execvp failed");
    exit(1);
} else if (pid > 0) {
    if (!cmd->background) {
        wait(NULL);
    }
} else {
    perror("fork failed");
}
```

**I/O Redirection**
```c
if (cmd->rstdin) {
    int fd = open(cmd->rstdin, O_RDONLY);
    if (fd < 0) { perror("open"); exit(1); }
    dup2(fd, STDIN_FILENO);
    close(fd);
}
```

**Pipe Setup**
```c
int pipefd[2];
pipe(pipefd);
// In writing process:
dup2(pipefd[1], STDOUT_FILENO);
close(pipefd[0]); close(pipefd[1]);
// In reading process:
dup2(pipefd[0], STDIN_FILENO);  
close(pipefd[0]); close(pipefd[1]);
```

**Signal Handler Setup**
```c
void sigint_handler(int sig) {
    // Handle Ctrl-C - don't exit shell
}
signal(SIGINT, sigint_handler);
```

Remember: Always check return values and handle errors appropriately!