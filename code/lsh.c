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
#include <sys/wait.h>

#include "parse.h"

static void print_cmd(Command *cmd); // Use Linked List to store commands
static void print_pgm(Pgm *p);
static int execute_cmd(Command *cmd);
void stripwhite(char *);

int main(void)
{
  for (;;)
  {
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
        print_cmd(&cmd);
        execute_cmd(&cmd);
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

  char *cmd_name = cmd->pgm->pgmlist[0];
  int argc = 0;
  while (cmd->pgm->pgmlist[argc] != NULL)
    argc++;

  if (strcmp(cmd_name, "exit") == 0)
  {
    if (argc > 1)
    {
      fprintf(stderr, "exit: too many arguments\n");
      return -1;
    }

    printf("\nexit\n");
    exit(0);
  }

  if (strcmp(cmd_name, "cd") == 0)
  {
    if (argc > 2)
    {
      fprintf(stderr, "cd: too many arguments\n");
      return -1;
    }

    char *path = cmd->pgm->pgmlist[1];
    if (path == NULL)
    {
      path = getenv("HOME"); // Default to HOME if no path provided
    }
    if (chdir(path) != 0)
    {
      perror("cd failed");
      return -1;
    }
    return 0;
  }

  Pgm *current_pgm = cmd->pgm;

  pid_t pid = fork();

  if (pid < 0) // Fork failed
  {
    perror("Fork failed");
    return -1;
  }

  if (pid == 0)
  {
    // execute command
    // WORKS ONLY FOR SIMPLE CMDS
    execvp(current_pgm->pgmlist[0], current_pgm->pgmlist);
  }

  else if (pid > 0)
  {
    if (cmd->background == 0)
    {
      signal(SIGINT, SIG_IGN); // Ignore SIGINT in parent
      wait(NULL);
      signal(SIGINT, SIG_DFL); // Restore default SIGINT handling after blocking wait
    }
    else
    {

      printf("Process running in background with PID: %d\n", pid);
      // No wait, background process
    }
  }

  // Ignore the piping for now

  return 0;
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
