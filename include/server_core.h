#ifndef SERVER_CORE_H
#define SERVER_CORE_H

#include <stdint.h>
#include <stddef.h>
#include "command.h"

typedef struct edit_log_entry {
    char *user;
    char *command;
    int result;
    struct edit_log_entry *next;
} edit_log_entry;

typedef struct server_core {
    document *doc;
    uint64_t current_version;

    edit_log_entry *pending_head;
    edit_log_entry *pending_tail;
    size_t pending_count;
} server_core;

server_core *server_core_init(void);
void server_core_free(server_core *core);

int server_core_submit(server_core *core,
                       const char *user,
                       user_role role,
                       const char *command_line);

char *server_core_commit(server_core *core);

char *server_core_flatten_doc(server_core *core);

#endif