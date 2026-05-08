#include "client_registry.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *registry_strdup(const char *s) {
    if (!s) {
        return NULL;
    }

    size_t n = strlen(s);
    char *r = malloc(n + 1);
    if (!r) {
        return NULL;
    }

    memcpy(r, s, n + 1);
    return r;
}

static void client_conn_free(client_conn *client) {
    if (!client) {
        return;
    }

    free(client->username);
    free(client->fifo_c2s_path);
    free(client->fifo_s2c_path);

    /*
     * The registry owns out_stream once the client is added.
     * In the real server this will close the FIFO stream.
     * In tests this closes the tmpfile stream.
     */
    if (client->out_stream) {
        fclose(client->out_stream);
    }

    free(client);
}

client_registry *client_registry_init(void) {
    client_registry *registry = calloc(1, sizeof(client_registry));
    return registry;
}

void client_registry_free(client_registry *registry) {
    if (!registry) {
        return;
    }

    client_conn *current = registry->head;
    while (current) {
        client_conn *next = current->next;

        if (current->fifo_c2s_path) {
            unlink(current->fifo_c2s_path);
        }

        if (current->fifo_s2c_path) {
            unlink(current->fifo_s2c_path);
        }

        client_conn_free(current);
        current = next;
    }

    free(registry);
}

client_conn *client_registry_find(client_registry *registry, pid_t pid) {
    if (!registry) {
        return NULL;
    }

    for (client_conn *cur = registry->head; cur; cur = cur->next) {
        if (cur->pid == pid) {
            return cur;
        }
    }

    return NULL;
}

int client_registry_add(client_registry *registry,
                        pid_t pid,
                        const char *username,
                        user_role role,
                        const char *fifo_c2s_path,
                        const char *fifo_s2c_path,
                        FILE *out_stream) {
    if (!registry || !username || !fifo_c2s_path || !fifo_s2c_path || !out_stream) {
        return -1;
    }

    if (client_registry_find(registry, pid)) {
        return -1;
    }

    client_conn *client = calloc(1, sizeof(client_conn));
    if (!client) {
        return -1;
    }

    client->pid = pid;
    client->role = role;
    client->username = registry_strdup(username);
    client->fifo_c2s_path = registry_strdup(fifo_c2s_path);
    client->fifo_s2c_path = registry_strdup(fifo_s2c_path);
    client->out_stream = out_stream;
    client->next = NULL;

    if (!client->username || !client->fifo_c2s_path || !client->fifo_s2c_path) {
        client_conn_free(client);
        return -1;
    }

    client->next = registry->head;
    registry->head = client;
    registry->count++;

    return 0;
}

int client_registry_remove(client_registry *registry, pid_t pid) {
    if (!registry) {
        return -1;
    }

    client_conn **pp = &registry->head;

    while (*pp) {
        client_conn *cur = *pp;

        if (cur->pid == pid) {
            *pp = cur->next;

            if (cur->fifo_c2s_path) {
                unlink(cur->fifo_c2s_path);
            }

            if (cur->fifo_s2c_path) {
                unlink(cur->fifo_s2c_path);
            }

            client_conn_free(cur);
            registry->count--;
            return 0;
        }

        pp = &cur->next;
    }

    return -1;
}

size_t client_registry_count(const client_registry *registry) {
    if (!registry) {
        return 0;
    }

    return registry->count;
}

int client_registry_broadcast(client_registry *registry, const char *payload) {
    if (!registry || !payload) {
        return -1;
    }

    for (client_conn *cur = registry->head; cur; cur = cur->next) {
        if (!cur->out_stream) {
            return -1;
        }

        if (fputs(payload, cur->out_stream) == EOF) {
            return -1;
        }

        if (fflush(cur->out_stream) == EOF) {
            return -1;
        }
    }

    return 0;
}