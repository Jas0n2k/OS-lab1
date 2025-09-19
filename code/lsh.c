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

#include "parse.h"

static void print_cmd(Command *cmd); // Use Linked List to store commands
static void print_pgm(Pgm *p);
static int execute_cmd(Command *cmd);
void stripwhite(char *);
static void execute_pipeline(Pgm *p, int cmd_idx, int num_cmds, int *pipefds, Command *cmd);

int main(void)
{
  for (;;)
  {
    char *line;
    line = readline("> ");
    if (line == NULL)
    {
      printf("\n");
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

static int execute_cmd(Command *cmd)
{
  if (cmd == NULL)
  {
    return -1; // Nothing to execute
  }

  Pgm *current_pgm = cmd->pgm;

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
  // Execute recursively
  execute_pipeline(current_pgm, 0, num_cmds, pipefds, cmd);

  // Wait for children
  if (!cmd->background)
  {
    for (int i = 0; i < num_cmds; i++)
      wait(NULL);
  }
  return 0;
}

// Recursive helper: execute from head to tail
static void execute_pipeline(Pgm *p, int cmd_idx, int num_cmds, int *pipefds, Command *cmd)
{
  if (p == NULL)
    return;

  // Recurse first to reach the earliest command
  execute_pipeline(p->next, cmd_idx + 1, num_cmds, pipefds, cmd);

  // Fork this command AFTER recursion unwinds
  pid_t pid = fork();
  assert(pid >= 0);

  if (pid == 0)
  {
    // read previous pipe if not first in pipeline
    if (cmd_idx < num_cmds - 1)
    {
      dup2(pipefds[2 * cmd_idx], STDIN_FILENO);
    }

    // Not the last in pipeline → write to next pipe
    if (cmd_idx > 0)
    {
      dup2(pipefds[2 * (cmd_idx - 1) + 1], STDOUT_FILENO);
    }
    execvp(p->pgmlist[0], p->pgmlist);
    perror("execvp");
    exit(1);
  }
  else
  {
    // Close used pipe fds in parent
    if (cmd_idx < num_cmds - 1)
      close(pipefds[2 * cmd_idx]);
    if (cmd_idx > 0)
      close(pipefds[2 * (cmd_idx - 1) + 1]);
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
