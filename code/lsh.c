/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "parse.h"

static void sigchld_handler(int sig);
static void print_cmd(Command *cmd); // Use Linked List to store commands
static void print_pgm(Pgm *p);
static int execute_cmd(Command *cmd);
void stripwhite(char *);
static int execute_pipeline(Pgm *p, int cmd_idx, int num_cmds, int *pipefds, Command *cmd);

static void sigchld_handler(int sig)
{
  // Reap all available zombie children
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int main(void)
{
  signal(SIGINT, SIG_IGN);          // Ignore Ctrl-C in the shell
  signal(SIGCHLD, sigchld_handler); // Handle child process termination
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
    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If stripped line not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Just prints cmd
        print_cmd(&cmd);
        execute_cmd(&cmd); // TO-DO
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Clear memory
    free(line);
  }

  return 0;
}

static int check_built_ins(Pgm *current_pgm, int argc)
{
  // Built-in command: cd
  if (strcmp(current_pgm->pgmlist[0], "cd") == 0)
  {
    // cd never takes more than 1 argument
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
  // Built-in command: exit
  else if (strcmp(current_pgm->pgmlist[0], "exit") == 0)
  {
    // exit never takes arguments
    if (argc > 1)
    {
      fprintf(stderr, "exit: too many arguments\n");
      return -1;
    }

    printf("\nexit\n");
    exit(0);
  }
  return 1;
}

static int execute_cmd(Command *cmd)
{
  if (cmd == NULL)
  {
    return -1; // Nothing to execute
  }

  char *cmd_name = cmd->pgm->pgmlist[0];
  int argc = 0;
  while (cmd->pgm->pgmlist[argc] != NULL)
    argc++;

  Pgm *current_pgm = cmd->pgm;

  if (check_built_ins(current_pgm, argc) != 0)
  {
    // Count commands
    int num_cmds = 0;
    for (Pgm *tmp = current_pgm; tmp; tmp = tmp->next)
      num_cmds++;

    // Create pipes
    // Each pipe needs 2 fds, and we need (num_cmds - 1) pipes
    int pipefds[2 * (num_cmds - 1)];
    for (int i = 0; i < num_cmds - 1; i++)
    {
      // create a pipe
      if (pipe(pipefds + 2 * i) < 0)
      {
        perror("pipe");
        exit(1);
      }
    }
    // Execute recursively
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
    return 0;
  }
}

// Recursive helper: execute from head to tail
static int execute_pipeline(Pgm *p, int cmd_idx, int num_cmds, int *pipefds, Command *cmd)
{
  if (p == NULL)
    return 0;

  // Recurse first to reach the earliest command
  execute_pipeline(p->next, cmd_idx + 1, num_cmds, pipefds, cmd);

  // Fork this command AFTER recursion unwinds
  pid_t pid = fork();

  if (pid < 0) // Fork failed
  {
    perror("Fork failed");
    return -1;
  }

  if (pid == 0)
  {
    // only set default signal handler for SIGINT if not background
    // background processes should ignore SIGINT
    if (!cmd->background)
      signal(SIGINT, SIG_DFL);

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

    // Close all pipe fds in child
    for (int i = 0; i < 2 * (num_cmds - 1); i++)
    {
      close(pipefds[i]);
    }

    execvp(p->pgmlist[0], p->pgmlist);
    perror("execvp");
    exit(1);
  }

  // Parent process continues to next iteration
  return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}
