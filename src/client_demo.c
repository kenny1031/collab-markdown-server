#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define FIFO_C2S "FIFO_C2S_DEMO"
#define FIFO_S2C "FIFO_S2C_DEMO"
#define LINE_BUF_SIZE 512

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void) {
    int c2s_fd = open(FIFO_C2S, O_WRONLY);
    if (c2s_fd == -1) {
        die("open FIFO_C2S_DEMO");
    }

    int s2c_fd = open(FIFO_S2C, O_RDONLY);
    if (s2c_fd == -1) {
        close(c2s_fd);
        die("open FIFO_S2C_DEMO");
    }

    FILE *c2s = fdopen(c2s_fd, "w");
    FILE *s2c = fdopen(s2c_fd, "r");

    if (!c2s || !s2c) {
        close(c2s_fd);
        close(s2c_fd);
        die("fdopen");
    }

    printf("Demo client connected.\n");
    printf("Type commands, e.g.:\n");
    printf("  INSERT 0 Hello\n");
    printf("  BOLD 0 5\n");
    printf("  QUIT\n\n");

    char input[LINE_BUF_SIZE];
    char output[LINE_BUF_SIZE];

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(s2c_fd, &read_fds);

        int max_fd = s2c_fd > STDIN_FILENO ? s2c_fd : STDIN_FILENO;

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready == -1) {
            die("select");
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!fgets(input, sizeof(input), stdin)) {
                break;
            }

            fputs(input, c2s);
            fflush(c2s);

            if (strcmp(input, "QUIT\n") == 0) {
                // Wait for server shutdown message.
            }
        }

        if (FD_ISSET(s2c_fd, &read_fds)) {
            if (!fgets(output, sizeof(output), s2c)) {
                break;
            }

            fputs(output, stdout);

            if (strcmp(output, "SERVER_SHUTDOWN\n") == 0) {
                break;
            }
        }
    }

    fclose(c2s);
    fclose(s2c);

    printf("Demo client stopped.\n");
    return 0;
}