# Shell program

The program is a shell program, and runs similarly to other \*nix based shell programs like bash, zshell etc. It is written in C, and required one external dependency (readline) to handle arrows for history.

### Run Instructions:
1. apt-get install libreadline-dev (If you don't have the readline library installed)
2. gcc shell_program.c -I/usr/include/readline -lreadline
3. ./a.out

### Implemented features:
1. Runs all native commands such as ls, pwd, grep etc
2. Pipes have been implemented (i.e. *command_1 | command_2*)
3. Background running possible (using *&* at the end, with a leading space)
4. Have to enter 'quit' to actually quit
5. History has been implemented. Saved locally in the file '.myhistory'
6. History features such as using upper/lower arrow to navigate commands, executing numbered command from history using ! (i.e. !*history_number*), and history followed by the number of commands to view (i.e. *history 5*) have been implemented as well.
