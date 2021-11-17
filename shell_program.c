/*
Name: Ash Rai

Need to install readline library first:
%  apt-get install libreadline-dev

To Run:
% gcc shell_program.c -I/usr/include/readline -lreadline
% ./a.out

Shell rules:
- Background running: Ampersand should be input at the end of the command with a
leading space (i.e. command &)
*/

#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define HISTORY_FILE_NAME ".myhistory"

int count_words(char *buffer);
int count_commands(char *buffer);
void split_tokens(char *buffer, char *token_array[], char *delimiter,
                  bool strip_whitespace);
void run_final_command(char *commands_array[], int commands_count, int fd[][2]);
void run_first_command(char *commands_array[], int fd[][2]);
void run_middle_command(char *commands_array[], int fd[][2], int command_index);
void run_single_command(char *buffer);
void run_history(char *buffer, int line_number, char history_commands[][256]);
bool is_history_command(char *buffer);
void run_numbered_history_command(char *buffer, int line_number,
                                  char history_commands[][256]);
void add_to_history(int *next_history_index, int max_history,
                    char history_commands[][256], char *buffer);
int run_command(char *token_array[], int words_count, int fd1[],
                int fd1_dup2_arg1, int fd1_dup2_arg2, int fd2[],
                int fd2_dup2_arg1, int fd2_dup2_arg2, int fd[][2], int count);
void quitHandler(int);
static int readline_func();
char *strip_whitespace(char *token);

int main(int argc, char *argv[]) {
    size_t size = 1024;
    signal(SIGINT, quitHandler);
    char *buffer = NULL;

    // history related variables
    FILE *fp = NULL;
    int max_history = 5;
    int max_command_size = 256;
    char history_commands[max_history][max_command_size];
    fp = fopen(HISTORY_FILE_NAME, "r");
    char line[size];
    int next_history_index = 0;

    // populate history commands array
    if (fp) {
        while (fgets(line, size, fp)) {
            line[strcspn(line, "\n")] = 0;
            add_history(line);
            strcpy(history_commands[next_history_index], line);
            next_history_index++;
        }
    }

    while (1) {
        // reading user input
        char shell_prompt[100];
        snprintf(shell_prompt, sizeof(shell_prompt), "enter data: ");
        buffer = readline(shell_prompt);

        // handle special inputs (i.e. exit, history)
        if (strcmp(buffer, "exit") == 0) {
            printf("OK close shop and go home\n");
            break;

        } else if (is_history_command(buffer)) {  // running history command
            add_to_history(&next_history_index, max_history, history_commands,
                           buffer);
            run_history(buffer, next_history_index, history_commands);
            continue;

        } else if (buffer[0] == '\n' || strlen(buffer) == 0) {  // empty input
            continue;

        } else if (buffer[0] ==
                   '!') {  // running code in with leading exclamation mark
            run_numbered_history_command(buffer, next_history_index,
                                         history_commands);
            if (is_history_command(buffer)) {
                add_to_history(&next_history_index, max_history,
                               history_commands, buffer);
                run_history(buffer, next_history_index, history_commands);
                continue;
            }

        } else if (strcmp(buffer, "erase history") == 0) {  // erase history
            int i = 0;
            memset(history_commands, 0, sizeof history_commands);
            next_history_index = 0;
            rl_clear_history();
            continue;
        }

        // add to history array
        add_to_history(&next_history_index, max_history, history_commands,
                       buffer);

        // split into different commands
        int commands_count = count_commands(buffer);
        char *commands_array[commands_count + 1];
        split_tokens(buffer, commands_array, "|", true);

        int i;
        if (commands_count > 1) {
            // for piped commands
            int fd[commands_count - 1][2];
            if (pipe(fd[0]) == -1) {
                perror("Error during pipe call");
            }

            // first command setup and run
            run_first_command(commands_array, fd);

            // sandwiched commands
            if (commands_count > 2) {
                for (int i = 0; i < (commands_count - 2); i++) {
                    if (pipe(fd[i + 1]) == -1) {
                        perror("Error during pipe call at command");
                    }
                    run_middle_command(commands_array, fd, i);
                }
            }

            // final command setup and run
            run_final_command(commands_array, commands_count, fd);
        } else {
            run_single_command(buffer);
        }
    }

    // write to history file
    fp = fopen(HISTORY_FILE_NAME, "w");
    for (int i = 0; i < next_history_index; i++) {
        fprintf(fp, history_commands[i]);
        fprintf(fp, "\n");
    }
    fclose(fp);
    free(buffer);
    return 0;
}

void add_to_history(int *next_history_index, int max_history,
                    char history_commands[][256], char *buffer) {
    if (*next_history_index == max_history) {
        rl_clear_history();
        // remove oldest command if max_history reached
        for (int i = 0; i < *next_history_index - 1; i++) {
            strcpy(history_commands[i], history_commands[i + 1]);
            add_history(history_commands[i + 1]);
        }
        strcpy(history_commands[*next_history_index - 1], buffer);

    } else {
        strcpy(history_commands[*next_history_index], buffer);
        (*next_history_index)++;
    }
    add_history(buffer);
    return;
}

void run_numbered_history_command(char *buffer, int line_number,
                                  char history_commands[][256]) {
    char *buffer_chopped = buffer + 1;
    buffer_chopped[strcspn(buffer_chopped, "\n")] = 0;
    int command_number = atoi(buffer_chopped);
    if (command_number > 0 && command_number <= line_number) {
        strcpy(buffer, history_commands[command_number - 1]);
    }
    return;
}

bool is_history_command(char *buffer) {
    return (
        strcmp(buffer, "history") == 0 ||
        ((strncmp(buffer, "history ", 8) == 0) && (count_words(buffer) == 2)));
}

void run_history(char *buffer, int line_number, char history_commands[][256]) {
    int i = 0;

    // if history XX input, then print out XX number of last commands
    if (strcmp(buffer, "history") != 0) {
        char *token;
        token = strtok(buffer, " ");
        if (token != NULL) {
            token = strtok(NULL, " ");
        }
        int commands_count = atoi(token);
        if (commands_count > 0) {
            i = line_number - commands_count;
            // if asked history commands number more than actual commands, print
            // all
            if (i < 0) {
                i = 0;
            }
        } else {
            return;
        }
    }

    if (i >= 0) {
        for (; i < line_number; i++) {
            printf("%i %s\n", i + 1, history_commands[i]);
        }
    }
    return;
}

void run_single_command(char *buffer) {
    // break user input into tokens and store into array
    int words_count = count_words(buffer);
    char *token_array[words_count + 1];
    split_tokens(buffer, token_array, " ", false);

    bool run_background =
        words_count > 1 && (strcmp(token_array[words_count - 1], "&") == 0);

    if (run_background) {
        token_array[words_count - 1] = NULL;
    } else {
        token_array[words_count] = NULL;
    }
    int pid = run_command(token_array, words_count, NULL, -1, -1, NULL, -1, -1,
                          NULL, -1);

    if (!run_background) {
        waitpid(pid, NULL, 0);
    }
    return;
}

void run_middle_command(char *commands_array[], int fd[][2],
                        int command_index) {
    int words_count = count_words(commands_array[command_index + 1]);
    char *token_array[words_count + 1];

    split_tokens(commands_array[command_index + 1], token_array, " ", false);
    token_array[words_count] = NULL;
    run_command(token_array, words_count, fd[command_index],
                fd[command_index][0], STDIN_FILENO, fd[command_index + 1],
                fd[command_index + 1][1], STDOUT_FILENO, fd, command_index + 2);
    return;
}

void run_first_command(char *commands_array[], int fd[][2]) {
    int words_count = count_words(commands_array[0]);
    char *token_array[words_count + 1];

    split_tokens(commands_array[0], token_array, " ", false);
    token_array[words_count] = NULL;
    run_command(token_array, words_count, fd[0], fd[0][1], STDOUT_FILENO, NULL,
                -1, -1, fd, 1);
    return;
}

void run_final_command(char *commands_array[], int commands_count,
                       int fd[][2]) {
    int words_count = count_words(commands_array[commands_count - 1]);
    char *token_array[words_count + 1];
    split_tokens(commands_array[commands_count - 1], token_array, " ", false);
    bool run_background =
        words_count > 1 && (strcmp(token_array[words_count - 1], "&") == 0);
    if (run_background) {
        token_array[words_count - 1] = NULL;
    } else {
        token_array[words_count] = NULL;
    }
    int pid = run_command(token_array, words_count, fd[commands_count - 2],
                          fd[commands_count - 2][0], STDIN_FILENO, fd[0], -1,
                          -1, fd, commands_count - 1);
    for (int i = 0; i < (commands_count - 1); i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }

    // wait if command process running in foreground
    if (!run_background) {
        waitpid(pid, NULL, 0);
    }
    return;
}

int count_words(char *buffer) {
    int words_count = 1;
    int i;
    for (i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ' ') {
            words_count++;
        }
    }
    return words_count;
}

int count_commands(char *buffer) {
    int commands_count = 1;
    int i;
    for (i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == '|') {
            commands_count++;
        }
    }
    return commands_count;
}

int run_command(char *token_array[], int words_count, int fd1[],
                int fd1_dup2_arg1, int fd1_dup2_arg2, int fd2[],
                int fd2_dup2_arg1, int fd2_dup2_arg2, int fd[][2], int count) {
    int pid = fork();
    if (pid < 0) {
        perror("Error while forking");
    } else if (pid == 0) {
        // child process
        fflush(stdout);

        dup2(fd1_dup2_arg1, fd1_dup2_arg2);
        if (fd2 != NULL && fd2_dup2_arg1 != -1) {
            dup2(fd2_dup2_arg1, fd2_dup2_arg2);
        }

        for (int i = 0; i < count; i++) {
            close(fd[i][0]);
            close(fd[i][1]);
        }

        if (execvp(token_array[0], token_array) < 0) {
            perror("Error in exec\n");
            exit(1);
        }
    }
    return pid;
}

void split_tokens(char *buffer, char *token_array[], char *delimiter,
                  bool strip_whitespace_bool) {
    int word_index = 0;
    char *token;
    token = strtok(buffer, delimiter);
    if (strip_whitespace_bool) {
        char *stripped_token = strip_whitespace(token);
        token_array[word_index] = stripped_token;
    } else {
        token_array[word_index] = token;
    }

    if (token != NULL) {
        token = strtok(NULL, delimiter);
        while (token != NULL) {
            word_index++;
            if (strip_whitespace_bool) {
                char *stripped_token = strip_whitespace(token);
                token_array[word_index] = stripped_token;
            } else {
                token_array[word_index] = token;
            }
            // token_array[word_index] = token;
            token = strtok(NULL, delimiter);
        }
    }
    return;
}

char *strip_whitespace(char *token) {
    char *stop;
    // moving pointer to strip leading whitespace
    while (isspace((unsigned char)*token)) {
        token++;
    }

    // inserting end char to trim trailing space
    stop = token + strlen(token) - 1;
    while (stop > token && isspace((unsigned char)*stop)) {
        stop--;
    }
    stop[1] = '\0';

    return token;
}

void quitHandler(int theInt) {
    printf("\nPlease input exit to exit.\n");
    fflush(stdout);
    printf("enter data: ");
    fflush(stdout);
    return;
}