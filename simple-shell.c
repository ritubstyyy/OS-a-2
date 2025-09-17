#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_TOKENS 64
#define DELIMITERS " \t\r\n"
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

/* Trim leading and trailing whitespace in-place. */
char *trim(char *s) {
    if (s == NULL) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s; // all spaces

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

/* Check if the command contains invalid characters (\ or "") */
int contains_invalid_chars(const char *s) {
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == '\\' || s[i] == '"') {
            return 1;
        }
    }
    return 0;
}

/* Add command to history (copy string) */
void add_history(const char *cmd) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    } else {
        // remove oldest, shift
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[HISTORY_SIZE - 1] = strdup(cmd);
    }
}

/* Print command history */
void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

/* Handle SIGINT (Ctrl-C) gracefully */
void sigint_handler(int signo) {
    (void)signo; // unused
    printf("\n[myshell] Press Ctrl-C or type 'exit' to quit\n");
    fflush(stdout);
}

int main(void) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    const char *prompt = "myshell> ";

    // Ignore Ctrl-C in shell, custom handler
    signal(SIGINT, sigint_handler);

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        read = getline(&line, &len, stdin);
        if (read == -1) { // EOF (Ctrl-D)
            putchar('\n');
            break;
        }

        char *cmd = trim(line);
        if (cmd[0] == '\0') continue;  // empty

        if (contains_invalid_chars(cmd)) {
            fprintf(stderr, "myshell: error - backslashes (\\) and double quotes (\") are not supported\n");
            continue;
        }

        add_history(cmd);

        // Built-ins
        if (strcmp(cmd, "exit") == 0) {
            break;
        }
        if (strcmp(cmd, "history") == 0) {
            print_history();
            continue;
        }

        // --- PARSING ---
        char *args[MAX_TOKENS];
        int i = 0;
        char *token = strtok(cmd, DELIMITERS);
        while (token != NULL && i < MAX_TOKENS - 1) {
            args[i++] = token;
            token = strtok(NULL, DELIMITERS);
        }
        args[i] = NULL;

        if (args[0] == NULL) continue;

        // --- EXECUTION ---
        pid_t pid = fork();
        if (pid == 0) {
            // Child restores default Ctrl-C behavior
            signal(SIGINT, SIG_DFL);

            if (execvp(args[0], args) == -1) {
                perror("myshell");
            }
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            wait(NULL);
        } else {
            perror("fork");
        }
    }

    // cleanup history
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
    free(line);
    return 0;
}
