#define _POSIX_C_SOURCE 200809L

#include "auth.h"
#include "client_registry.h"
#include "server_core.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef SIGRTMIN
#define SERVER_CONNECT_SIGNAL SIGUSR1
#define CLIENT_READY_SIGNAL SIGUSR2
#else
#define SERVER_CONNECT_SIGNAL SIGRTMIN
#define CLIENT_READY_SIGNAL (SIGRTMIN + 1)
#endif

#define FIFO_PATH_SIZE 128
#define USERNAME_BUF_SIZE 128
#define ROLES_PATH "config/roles.txt"

typedef struct client_thread_args {
    pid_t client_pid;
    server_core *core;
    client_registry *registry;
    pthread_mutex_t *mutex;
} client_thread_args;

static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t connect_received = 0;
static volatile sig_atomic_t pending_client_pid = 0;

static void connect_signal_handler(int signo, siginfo_t *info, void *context) {
    (void)signo;
    (void)context;

    if (info) {
        pending_client_pid = info->si_pid;
        connect_received = 1;
    }
}

static void cleanup_fifo_pair(const char *c2s_path, const char *s2c_path) {
    if (c2s_path) {
        unlink(c2s_path);
    }

    if (s2c_path) {
        unlink(s2c_path);
    }
}

static void build_fifo_paths(pid_t client_pid,
                             char *c2s_path,
                             size_t c2s_size,
                             char *s2c_path,
                             size_t s2c_size) {
    snprintf(c2s_path, c2s_size, "FIFO_C2S_%d", (int)client_pid);
    snprintf(s2c_path, s2c_size, "FIFO_S2C_%d", (int)client_pid);
}

static int make_fifo_pair(const char *c2s_path, const char *s2c_path) {
    cleanup_fifo_pair(c2s_path, s2c_path);

    if (mkfifo(c2s_path, 0600) == -1) {
        return -1;
    }

    if (mkfifo(s2c_path, 0600) == -1) {
        unlink(c2s_path);
        return -1;
    }

    return 0;
}

static char *read_line_strip_newline(FILE *stream, char *buf, size_t size) {
    if (!fgets(buf, size, stream)) {
        return NULL;
    }

    size_t n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
    }

    return buf;
}

static const char *role_to_wire_string(user_role role) {
    if (role == ROLE_WRITE) {
        return "write";
    }

    return "read";
}

static int send_initial_document(FILE *out, user_role role, server_core *core) {
    char *doc_text = server_core_flatten_doc(core);
    if (!doc_text) {
        return -1;
    }

    size_t doc_len = strlen(doc_text);

    fprintf(out, "%s\n", role_to_wire_string(role));
    fprintf(out, "%llu\n", (unsigned long long)core->current_version);
    fprintf(out, "%zu\n", doc_len);

    if (doc_len > 0) {
        fwrite(doc_text, 1, doc_len, out);
    }

    fflush(out);
    free(doc_text);

    return 0;
}

static void *client_thread_main(void *arg) {
    client_thread_args *args = arg;

    pid_t client_pid = args->client_pid;
    server_core *core = args->core;
    client_registry *registry = args->registry;
    pthread_mutex_t *mutex = args->mutex;

    char c2s_path[FIFO_PATH_SIZE];
    char s2c_path[FIFO_PATH_SIZE];

    build_fifo_paths(client_pid,
                     c2s_path,
                     sizeof(c2s_path),
                     s2c_path,
                     sizeof(s2c_path));

    if (make_fifo_pair(c2s_path, s2c_path) == -1) {
        perror("mkfifo");
        free(args);
        return NULL;
    }

    if (kill(client_pid, CLIENT_READY_SIGNAL) == -1) {
        perror("kill client ready signal");
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    int c2s_fd = open(c2s_path, O_RDONLY);
    if (c2s_fd == -1) {
        perror("open C2S");
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    int s2c_fd = open(s2c_path, O_WRONLY);
    if (s2c_fd == -1) {
        perror("open S2C");
        close(c2s_fd);
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    FILE *c2s = fdopen(c2s_fd, "r");
    FILE *s2c = fdopen(s2c_fd, "w");

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

        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    char username[USERNAME_BUF_SIZE];

    if (!read_line_strip_newline(c2s, username, sizeof(username))) {
        fclose(c2s);
        fclose(s2c);
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    user_role role;
    int auth_result = auth_lookup_role(ROLES_PATH, username, &role);

    if (auth_result != AUTH_OK) {
        fprintf(s2c, "Reject UNAUTHORISED\n");
        fflush(s2c);

        sleep(1);

        fclose(c2s);
        fclose(s2c);
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    pthread_mutex_lock(mutex);

    if (send_initial_document(s2c, role, core) == -1) {
        pthread_mutex_unlock(mutex);

        fclose(c2s);
        fclose(s2c);
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    if (client_registry_add(registry,
                            client_pid,
                            username,
                            role,
                            c2s_path,
                            s2c_path,
                            s2c) == -1) {
        pthread_mutex_unlock(mutex);

        fclose(c2s);
        /*
         * s2c is not owned by registry if add fails.
         */
        fclose(s2c);
        cleanup_fifo_pair(c2s_path, s2c_path);
        free(args);
        return NULL;
    }

    pthread_mutex_unlock(mutex);

    printf("Client connected: pid=%d user=%s role=%s\n",
           (int)client_pid,
           username,
           role_to_wire_string(role));
    fflush(stdout);

    char command_line[512];

    while (fgets(command_line, sizeof(command_line), c2s)) {
        if (strcmp(command_line, "DISCONNECT\n") == 0 ||
            strcmp(command_line, "QUIT\n") == 0) {
            break;
            }

        pthread_mutex_lock(mutex);

        int result = server_core_submit(core, username, role, command_line);
        char *payload = server_core_commit(core);

        if (payload) {
            client_registry_broadcast(registry, payload);
            free(payload);
        }

        pthread_mutex_unlock(mutex);

        printf("Command from %s: %sResult: %s\n",
               username,
               command_line,
               command_result_to_string(result));
        fflush(stdout);
    }

    pthread_mutex_lock(mutex);
    client_registry_remove(registry, client_pid);
    pthread_mutex_unlock(mutex);

    fclose(c2s);

    printf("Client disconnected: pid=%d user=%s\n", (int)client_pid, username);
    fflush(stdout);

    free(args);
    return NULL;
}

static int parse_interval_ms(const char *s, int *out) {
    if (!s || !out) {
        return 0;
    }

    char *end = NULL;
    long value = strtol(s, &end, 10);

    if (*end != '\0' || value <= 0) {
        return 0;
    }

    *out = (int)value;
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <TIME_INTERVAL_MS>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int interval_ms = 0;
    if (!parse_interval_ms(argv[1], &interval_ms)) {
        fprintf(stderr, "Invalid TIME_INTERVAL_MS\n");
        return EXIT_FAILURE;
    }

    server_core *core = server_core_init();
    if (!core) {
        fprintf(stderr, "Failed to initialise server core\n");
        return EXIT_FAILURE;
    }

    client_registry *registry = client_registry_init();
    if (!registry) {
        server_core_free(core);
        fprintf(stderr, "Failed to initialise client registry\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = connect_signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SERVER_CONNECT_SIGNAL, &sa, NULL) == -1) {
        perror("sigaction");
        client_registry_free(registry);
        server_core_free(core);
        pthread_mutex_destroy(&mutex);
        return EXIT_FAILURE;
    }

    printf("Server PID: %d\n", (int)getpid());
    fflush(stdout);

    printf("Using update interval: %d ms\n", interval_ms);
    fflush(stdout);

    while (keep_running) {
        pause();

        if (!connect_received) {
            continue;
        }

        pid_t client_pid = (pid_t)pending_client_pid;

        connect_received = 0;
        pending_client_pid = 0;

        if (client_pid <= 0) {
            continue;
        }

        client_thread_args *args = malloc(sizeof(client_thread_args));
        if (!args) {
            perror("malloc client_thread_args");
            continue;
        }

        args->client_pid = client_pid;
        args->core = core;
        args->registry = registry;
        args->mutex = &mutex;

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, client_thread_main, args);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
            free(args);
            continue;
        }

        pthread_detach(tid);
    }

    pthread_mutex_lock(&mutex);
    size_t clients = client_registry_count(registry);
    pthread_mutex_unlock(&mutex);

    if (clients > 0) {
        printf("Server exiting with %zu connected clients\n", clients);
    }

    client_registry_free(registry);
    server_core_free(core);
    pthread_mutex_destroy(&mutex);

    return EXIT_SUCCESS;
}