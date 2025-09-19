In which order?
We started which are run directly with execvp such as ls and date. We then implemented the in built-in functions such as cd and exit. We then split up the I/O redirection, background processes and pipes. This proved difficult since especially background processes are closely linked to how you handle the two others. Therefore there was a large amount of collaboration between the group. We then finally implemented the ctrl + c or SIGINT behavior. This was quite simple but we questioned the behavior towards background processes but finally decided that it should only interrupt foreground processes which proved true when the tests then passed.

# Lab Report: Custom Shell Implementation

## Meeting the Specifications

We successfully implemented all eight specifications required by the lab and confirmed our solution by passing all thirteen automated test cases.

Our development process followed this sequence:

1. Implemented basic external commands using `execvp` (e.g., `ls`, `date`).
2. Added built-in commands such as `cd` and `exit`.
3. Extended the shell with input/output redirection.
4. Implemented support for background processes.
5. Implemented pipelines, ensuring proper communication between multiple commands.
6. Added signal handling for `SIGINT` (Ctrl+C), so that only foreground processes are interrupted.

---

## Challenges Encountered

One of the main difficulties when implementing pipes was to understand how pipes work and how the parser had placed the commands. It was really helpful to have the `print_pgm(Pgm \*p)` function to study as the recursive structure was kept when doing the pipes. It was also a small challenge to understand the desired behavior of pipes since we had limited experience using them.

Another challenge was handling background processes, since they are closely related to both I/O redirection and pipelines. Coordinating these features required significant debugging and teamwork. Finally, deciding the correct behavior for `SIGINT` took some thought. We concluded that only foreground processes should be interrupted, which was confirmed by the automated tests.

---

## Automated Tests

The automated tests were highly useful. They provided immediate feedback, clarified the expected behavior of the shell, and ensured we were on the right track during development. Passing all thirteen tests gave us confidence that our implementation met the assignment requirements.

---

## Missing Tests / Feedback

The description for some of the test was a bit misleading but this was perhaps because we encountered some edge cases which triggered the assert but was not the main test for this assert.

## Conclusion

This lab was both challenging and rewarding. We gained valuable experience with processes, file descriptors, pipes, and signals in C. The recursive structure of the parser also deepened our understanding of how shells manage commands. The project required close collaboration and problem solving, and in the end, we are satisfied with the robustness of our solution.
