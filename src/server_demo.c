#include "server_core.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIFO_C2S "FIFO_C2S_DEMO"
#define FIFO_S2C "FIFO_S2C_DEMO"
#define LINE_BUF_SIZE 512

static void cleanup_fifos(void) {
    unlink(FIFO_C2S);
    unlink(FIFO_S2C);
}

static void die(const char *msg) {
    perror(msg);
    cleanup_fifos();
    exit(EXIT_FAILURE);
}

int main(void) {
    cleanup_fifos();

    if (mkfifo(FIFO_C2S, 0600) == -1) {
        die("mkfifo FIFO_C2S_DEMO");
    }

    if (mkfifo(FIFO_S2C, 0600) == -1) {
        die("mkfifo FIFO_S2C_DEMO");
    }

    printf("Demo server started.\n");
    printf("C2S FIFO: %s\n", FIFO_C2S);
    printf("S2C FIFO: %s\n", FIFO_S2C);
    printf("Waiting for demo client...\n");
    fflush(stdout);

    int c2s_fd = open(FIFO_C2S, O_RDONLY);
    if (c2s_fd == -1) {
        die("open c2s");
    }

    int s2c_fd = open(FIFO_S2C, O_WRONLY);
    if (s2c_fd == -1) {
        close(c2s_fd);
        die("open s2c");
    }

    FILE *c2s = fdopen(c2s_fd, "r");
    FILE *s2c = fdopen(s2c_fd, "w");

    if (!c2s || !s2c) {
        close(c2s_fd);
        close(s2c_fd);
        die("fdopen");
    }

    server_core *core = server_core_init();
    if (!core) {
        die("server_core_init");
    }

    fprintf(s2c, "CONNECTED demo-user write\n");
    fflush(s2c);

    char line[LINE_BUF_SIZE];

    while (fgets(line, sizeof(line), c2s)) {
        if (strcmp(line, "QUIT\n") == 0) {
            fprintf(s2c, "SERVER_SHUTDOWN\n");
            fflush(s2c);
            break;
        }

        int result = server_core_submit(core, "demo-user", ROLE_WRITE, line);
        char *payload = server_core_commit(core);

        if (!payload) {
            fprintf(s2c, "ERROR failed to build broadcast payload\n");
            fflush(s2c);
            continue;
        }

        fprintf(s2c, "%s", payload);

        char *doc = server_core_flatten_doc(core);
        if (doc) {
            fprintf(s2c, "DOC_BEGIN\n%s\nDOC_END\n", doc);
            free(doc);
        }

        fflush(s2c);

        printf("Executed command: %sResult: %s\n",
               line,
               command_result_to_string(result));
        fflush(stdout);

        free(payload);
    }

    server_core_free(core);

    fclose(c2s);
    fclose(s2c);

    cleanup_fifos();

    printf("Demo server stopped.\n");
    return 0;
}