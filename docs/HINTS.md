** Tasks

* What we already have
- History of commands
- we have a parser
- Build commands


* What we have to do
- Ctrl-D to exit
- Build execute_cmd function to run the commands
 1. Determine if the command is valid or is cd / exit;
 2. Get the program we are gonna run (main part of coding, i.g. ls/date/who);
 3. Fork a child process to run the program seperately, while the main proceess waits for a return;