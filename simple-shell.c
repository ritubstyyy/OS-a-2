#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* Trim leading and trailing whitespace in-place.
 * Returns pointer to trimmed string (may point inside original buffer).
 */
char *trim(char *s) {
    if (s == NULL) return NULL;

    // Trim leading
    while (isspace((unsigned char)*s)) s++;

    if (*s == '\0') return s; // all spaces

    // Trim trailing
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

int main(void) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    const char *prompt = "myshell> ";

    while (1) {
        // Print prompt (use fflush to ensure it shows before getline)
        printf("%s", prompt);
        fflush(stdout);

        read = getline(&line, &len, stdin);
        if (read == -1) { // EOF or error (e.g., Ctrl-D)
            // Clean exit
            putchar('\n');
            break;
        }

        // Trim newline and surrounding whitespace
        char *cmd = trim(line);

        // If empty, continue
        if (cmd[0] == '\0') {
            continue;
        }

        // Quick developer/testing command: exit
        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        // For now: echo back the command (later we'll parse & execute)
        printf("[debug] received command: \"%s\"\n", cmd);

        // In later steps: store cmd in history (timestamp), parse tokens, handle pipes, execute etc.
    }

    free(line);
    return 0;
}
