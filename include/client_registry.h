#ifndef CLIENT_REGISTRY_H
#define CLIENT_REGISTRY_H

#include <stdio.h>
#include <sys/types.h>
#include "command.h"

typedef struct client_conn {
    pid_t pid;
    char *username;
    user_role role;

    char *fifo_c2s_path;
    char *fifo_s2c_path;

    FILE *out_stream;

    struct client_conn *next;
} client_conn;

typedef struct client_registry {
    client_conn *head;
    size_t count;
} client_registry;

client_registry *client_registry_init(void);
void client_registry_free(client_registry *registry);

int client_registry_add(client_registry *registry,
                        pid_t pid,
                        const char *username,
                        user_role role,
                        const char *fifo_c2s_path,
                        const char *fifo_s2c_path,
                        FILE *out_stream);

int client_registry_remove(client_registry *registry, pid_t pid);

client_conn *client_registry_find(client_registry *registry, pid_t pid);

size_t client_registry_count(const client_registry *registry);

int client_registry_broadcast(client_registry *registry, const char *payload);

#endif