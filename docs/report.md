# Lab 1 Report: Custom Shell Implementation

## Author
- Nils Hugoson
- Pengfei Li
- Isak Söderlind

## Meeting the Specifications

We successfully implemented all eight specifications required by the lab.
And we confirmed our solution valid by passing all thirteen automated test cases.

## Development Process & Challenges

Our development process followed this sequence:

1. Implemented basic external commands (e.g., `ls`, `date`) using the function `execvp`. *(By all members during online meetings)*
2. Added built-in commands such as `cd` and `exit`. *(By Pengfei and Isak)*
3. Implemented **pipelines**, ensuring proper communication between multiple commands. *(By Nils)*
4. Extended the shell with **I/O redirection**. *(By Nils)*
5. Implemented support for **background processes**. *(By Pengfei and Isak)*
7. Added signal handling for `SIGINT` (Ctrl+C), so that only foreground processes are interrupted. *(By Nils)*
8. Wrapped handlers in dedicated functions and formatted the code. *(By Isak and Nils)*
9. Debugged by running the provided tests and fixed problems with the zombie process. *(By all members via online meeting)*

### Pipelining

The part of **pipes** was initially built by Nils. It is provided in the documentation that we shall respect the 'last-to-first' execution order. Let us make an example.
``` bash
$ ls | grep lsh.c | wc
```
We did not understand this concept in the first place, since we thought the pipeline execution was from left to right. (`ls` first, then `grep`, then `wc`)
However, we soon realized that the pipeline did not go as we had planned. `ls` might start producing some output, even before `grep` was ready to receive it. This caused some bugs.

Then Nils took another approch, by flipping the execution order to 'last-to-first'. In general, the example above works like this:
  1. `wc` process is created first (ready to read from pipe)
  2. `grep` process is created second (ready to read from `ls` and write to `wc`)
  3. `ls` process is created last (ready to write to `grep`)

One of the main difficulties when implementing pipes was to understand how pipes really work, and how the provided parser places the commands. It was really helpful to have the `print_pgm()` function that we studied its recursive structure for building the pipes. It was also a small challenge to understand the desired behavior of pipes since we had limited experience using them.

### I/O Redirection
I/O redirection is quite straight-forward when we understood how the pipes work. Similarily to the previous resolution, we managed to implement the I/O redirection using the function `open()`, which is for obtaining the file descriptor in the pipelining.

### Background Process
Another challenge was handling background processes, since they are closely related to both I/O redirection and pipelines. Coordinating these features required significant debugging and teamwork. Therefore, a large amount of collaboration between the group members involved.

First we found that killing the background processes seemed impossible, and it was because we did not quite understand how we should pass the `Ctrl + C` to the background process. After digging into the process management in the operating systems, we realized that we did not have to do that. By terminating the parent process, all background child processes will be handled by the OS and their parent will be a system process, usually `init.d` in linux.

Deciding the correct behavior for `SIGINT` took some thought. Implementation was actually quite simple, but we questioned the behavior towards background processes but finally decided that it should only interrupt foreground processes which proved true when the tests then passed.

## Automated Tests

We concluded that only foreground processes should be interrupted, which was confirmed by the automated tests.

The automated tests were highly useful. They provided immediate feedback, detailed error logs, clarified the expected behavior of the shell, and ensured we were on the right track during development. Passing all thirteen tests gave us confidence that our implementation met the assignment requirements.


### Feedback
The description text for some of the test was a bit misleading. Perhaps this was because we encountered some edge cases which triggered the assert, but it was not the main purpose for the test.

## Conclusion

This lab was both challenging and rewarding. We gained valuable experience with processes, file descriptors, pipes, and signals in C. The recursive structure of the parser also deepened our understanding of how shells manage commands. The project required close collaboration and problem solving, and in the end, we are satisfied with the robustness of our solution.