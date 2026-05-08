#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#ifndef SIGRTMIN
#define SERVER_CONNECT_SIGNAL SIGUSR1
#define CLIENT_READY_SIGNAL SIGUSR2
#else
#define SERVER_CONNECT_SIGNAL SIGRTMIN
#define CLIENT_READY_SIGNAL (SIGRTMIN + 1)
#endif

#define FIFO_PATH_SIZE 128
#define LINE_BUF_SIZE 256

static void build_fifo_paths(pid_t client_pid,
                             char *c2s_path,
                             size_t c2s_size,
                             char *s2c_path,
                             size_t s2c_size) {
    snprintf(c2s_path, c2s_size, "FIFO_C2S_%d", (int)client_pid);
    snprintf(s2c_path, s2c_size, "FIFO_S2C_%d", (int)client_pid);
}

static int parse_pid(const char *s, pid_t *out) {
    if (!s || !out) {
        return 0;
    }

    char *end = NULL;
    long value = strtol(s, &end, 10);

    if (*end != '\0' || value <= 0) {
        return 0;
    }

    *out = (pid_t)value;
    return 1;
}

static int read_line(FILE *stream, char *buf, size_t size) {
    if (!fgets(buf, size, stream)) {
        return 0;
    }

    return 1;
}

static int read_exact(FILE *stream, char *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        size_t n = fread(buf + total, 1, len - total, stream);

        if (n == 0) {
            if (ferror(stream)) {
                return 0;
            }

            return 0;
        }

        total += n;
    }

    return 1;
}

static void strip_newline(char *s) {
    size_t n = strlen(s);

    if (n > 0 && s[n - 1] == '\n') {
        s[n - 1] = '\0';
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_pid> <username>\n", argv[0]);
        return EXIT_FAILURE;
    }

    pid_t server_pid;
    if (!parse_pid(argv[1], &server_pid)) {
        fprintf(stderr, "Invalid server_pid\n");
        return EXIT_FAILURE;
    }

    const char *username = argv[2];

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, CLIENT_READY_SIGNAL);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    if (kill(server_pid, SERVER_CONNECT_SIGNAL) == -1) {
        perror("kill server connect signal");
        return EXIT_FAILURE;
    }

    int received_signal = 0;
    if (sigwait(&set, &received_signal) != 0) {
        perror("sigwait");
        return EXIT_FAILURE;
    }

    if (received_signal != CLIENT_READY_SIGNAL) {
        fprintf(stderr, "Unexpected signal received\n");
        return EXIT_FAILURE;
    }

    pid_t client_pid = getpid();

    char c2s_path[FIFO_PATH_SIZE];
    char s2c_path[FIFO_PATH_SIZE];

    build_fifo_paths(client_pid,
                     c2s_path,
                     sizeof(c2s_path),
                     s2c_path,
                     sizeof(s2c_path));

    int c2s_fd = open(c2s_path, O_WRONLY);
    if (c2s_fd == -1) {
        perror("open C2S FIFO");
        return EXIT_FAILURE;
    }

    int s2c_fd = open(s2c_path, O_RDONLY);
    if (s2c_fd == -1) {
        perror("open S2C FIFO");
        close(c2s_fd);
        return EXIT_FAILURE;
    }

    FILE *c2s = fdopen(c2s_fd, "w");
    FILE *s2c = fdopen(s2c_fd, "r");

    if (!c2s || !s2c) {
        perror("fdopen");

        if (c2s) {
            fclose(c2s);
        } else {
            close(c2s_fd);
        }

        if (s2c) {
            fclose(s2c);
        } else {
            close(s2c_fd);
        }

        return EXIT_FAILURE;
    }

    fprintf(c2s, "%s\n", username);
    fflush(c2s);

    char line[LINE_BUF_SIZE];

    if (!read_line(s2c, line, sizeof(line))) {
        fprintf(stderr, "Failed to read role/rejection\n");
        fclose(c2s);
        fclose(s2c);
        return EXIT_FAILURE;
    }

    strip_newline(line);

    if (strncmp(line, "Reject", 6) == 0) {
        printf("%s\n", line);
        fclose(c2s);
        fclose(s2c);
        return EXIT_FAILURE;
    }

    char role[LINE_BUF_SIZE];
    snprintf(role, sizeof(role), "%s", line);

    if (!read_line(s2c, line, sizeof(line))) {
        fprintf(stderr, "Failed to read version\n");
        fclose(c2s);
        fclose(s2c);
        return EXIT_FAILURE;
    }

    strip_newline(line);
    unsigned long long version = strtoull(line, NULL, 10);

    if (!read_line(s2c, line, sizeof(line))) {
        fprintf(stderr, "Failed to read document length\n");
        fclose(c2s);
        fclose(s2c);
        return EXIT_FAILURE;
    }

    strip_newline(line);
    size_t doc_len = (size_t)strtoull(line, NULL, 10);

    char *doc = malloc(doc_len + 1);
    if (!doc) {
        fclose(c2s);
        fclose(s2c);
        return EXIT_FAILURE;
    }

    if (doc_len > 0) {
        if (!read_exact(s2c, doc, doc_len)) {
            fprintf(stderr, "Failed to read document body\n");
            free(doc);
            fclose(c2s);
            fclose(s2c);
            return EXIT_FAILURE;
        }
    }

    doc[doc_len] = '\0';

    printf("Connected as %s\n", username);
    printf("Role: %s\n", role);
    printf("Version: %llu\n", version);
    printf("Document length: %zu\n", doc_len);
    printf("Document:\n%s\n", doc);
    printf("\nType commands, e.g.:\n");
    printf("  INSERT 0 Hello\n");
    printf("  BOLD 0 5\n");
    printf("  DEL 0 2\n");
    printf("  DISCONNECT\n\n");
    printf("> ");
    fflush(stdout);

    free(doc);

    char input[512];
    char output[512];

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(s2c_fd, &read_fds);

        int max_fd = s2c_fd > STDIN_FILENO ? s2c_fd : STDIN_FILENO;

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready == -1) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!fgets(input, sizeof(input), stdin)) {
                break;
            }

            fputs(input, c2s);
            fflush(c2s);

            if (strcmp(input, "DISCONNECT\n") == 0 ||
                strcmp(input, "QUIT\n") == 0) {
                break;
                }
        }

        if (FD_ISSET(s2c_fd, &read_fds)) {
            if (!fgets(output, sizeof(output), s2c)) {
                break;
            }

            printf("\n[server] %s", output);

            /*
             * Print the rest of the broadcast payload until END.
             */
            if (strncmp(output, "VERSION ", 8) == 0) {
                while (fgets(output, sizeof(output), s2c)) {
                    printf("[server] %s", output);
                    if (strcmp(output, "END\n") == 0) {
                        break;
                    }
                }
            }

            printf("> ");
            fflush(stdout);
        }
    }

    fclose(c2s);
    fclose(s2c);

    printf("Disconnected.\n");
    return EXIT_SUCCESS;
}