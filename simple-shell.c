#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define MAX_TOKENS 64
#define HISTORY_SIZE 100

typedef struct {
    char *command;
    pid_t pid;
    struct timeval start, end;
} HistoryEntry;

HistoryEntry history[HISTORY_SIZE];
int history_count = 0;

//trim leading and trailing whitespace 
char *trim(char *s) {
    while (isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return s;
    }

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }

    return s;
}

//check for invalid characters 
int contains_invalid_chars(const char *s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\\' || s[i] == '"' || s[i] == '\t') {
            return 1;
        }
    }
    return 0;
}

//signal handler for Ctrl+C 
void handle_sigint(int sig) {
    (void)sig;
    printf("\nUse \"exit\" or Ctrl+D to quit\n");
}

//parse command into arguments (no malloc needed) 
int parse_args(char *cmd, char **args) {
    int i = 0;

    char *token = strtok(cmd, " ");
    while (token && i < MAX_TOKENS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    return i; //number of arguments
}

// Execute commands with optional pipes
void execute_pipeline(char *line) {
    if (history_count >= HISTORY_SIZE) {
        fprintf(stderr, "History full, cannot store more commands\n");
        return;
    }

    char *commands[MAX_TOKENS];
    int n = 0;

    //split by '|'
    char *cmd = strtok(line, "|");
    while (cmd && n < MAX_TOKENS - 1) {
        commands[n++] = trim(cmd);
        cmd = strtok(NULL, "|");
    }
    commands[n] = NULL;

    int pipes[2 * (n - 1)];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes + i * 2) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    pid_t pids[MAX_TOKENS] = {0};

    for (int i = 0; i < n; i++) {
        gettimeofday(&history[history_count].start, NULL);

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            //child process
            if (i > 0) {
                dup2(pipes[(i - 1) * 2], 0);  //read from previous pipe
            }

            if (i < n - 1) {
                dup2(pipes[i * 2 + 1], 1);    //write to next pipe
            }

            //close all pipe fds in child
            for (int j = 0; j < 2 * (n - 1); j++) {
                close(pipes[j]);
            }

            char *args[MAX_TOKENS];
            parse_args(commands[i], args);
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else {
            //parent process
            history[history_count].pid = pid;
            history[history_count].command = strdup(commands[i]);
            pids[i] = pid;
        }
    }

    //close all pipes in parent
    for (int j = 0; j < 2 * (n - 1); j++) {
        close(pipes[j]);
    }

    //wait for all children
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
        gettimeofday(&history[history_count].end, NULL);
        history_count++;
    }
}

//print execution log 
void print_log() {
    printf("\n=== Execution Log ===\n");

    for (int i = 0; i < history_count; i++) {
        struct timeval diff;
        timersub(&history[i].end, &history[i].start, &diff);

        char tbuf[64];
        time_t sec = history[i].start.tv_sec;
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&sec));

        printf("Command: %s\n", history[i].command);
        printf("  PID: %d\n", history[i].pid);
        printf("  Start: %s\n", tbuf);
        printf("  Duration: %ld.%06lds\n\n",
               (long)diff.tv_sec, (long)diff.tv_usec);
    }
}

int main() {
    char *line = NULL;
    size_t len = 0;

    signal(SIGINT, handle_sigint);

    while (1) {
        printf("myshell> ");
        fflush(stdout);

        if (getline(&line, &len, stdin) == -1) {
            putchar('\n');
            break;
        }

        char *cmd = trim(line);

        if (*cmd == '\0') {
            continue;
        }

        if (contains_invalid_chars(cmd)) {
            fprintf(stderr, "Invalid characters in command\n");
            continue;
        }

        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        //display command history
        if (strcmp(cmd, "history") == 0) {
            for (int i = 0; i < history_count; i++) {
                printf("%d: %s\n", i + 1, history[i].command);
            }
            continue;
        }

        execute_pipeline(cmd);
    }

    print_log();

    free(line);

    for (int i = 0; i < history_count; i++) {
        free(history[i].command);
    }

    return 0;
}