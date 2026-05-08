#include "server_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *core_strdup(const char *s) {
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

static char *copy_without_trailing_newline(const char *s) {
    if (!s) {
        return NULL;
    }

    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') {
        n--;
    }

    char *r = malloc(n + 1);
    if (!r) {
        return NULL;
    }

    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static void free_log_entries(edit_log_entry *entry) {
    while (entry) {
        edit_log_entry *next = entry->next;
        free(entry->user);
        free(entry->command);
        free(entry);
        entry = next;
    }
}

static int append_log_entry(server_core *core,
                            const char *user,
                            const char *command_line,
                            int result) {
    edit_log_entry *entry = malloc(sizeof(edit_log_entry));
    if (!entry) {
        return -1;
    }

    entry->user = core_strdup(user);
    entry->command = copy_without_trailing_newline(command_line);
    entry->result = result;
    entry->next = NULL;

    if (!entry->user || !entry->command) {
        free(entry->user);
        free(entry->command);
        free(entry);
        return -1;
    }

    if (core->pending_tail) {
        core->pending_tail->next = entry;
    } else {
        core->pending_head = entry;
    }

    core->pending_tail = entry;
    core->pending_count++;

    return 0;
}

server_core *server_core_init(void) {
    server_core *core = calloc(1, sizeof(server_core));
    if (!core) {
        return NULL;
    }

    core->doc = markdown_init();
    if (!core->doc) {
        free(core);
        return NULL;
    }

    core->current_version = 0;
    return core;
}

void server_core_free(server_core *core) {
    if (!core) {
        return;
    }

    markdown_free(core->doc);
    free_log_entries(core->pending_head);
    free(core);
}

int server_core_submit(server_core *core,
                       const char *user,
                       user_role role,
                       const char *command_line) {
    if (!core || !user || !command_line) {
        return CMD_REJECT_INVALID_COMMAND;
    }

    int result = command_execute(core->doc,
                                 core->current_version,
                                 role,
                                 command_line);

    if (append_log_entry(core, user, command_line, result) != 0) {
        return CMD_REJECT_INVALID_COMMAND;
    }

    return result;
}

static size_t broadcast_payload_size(const server_core *core,
                                     uint64_t next_version) {
    size_t total = 0;

    total += snprintf(NULL, 0, "VERSION %llu\n",
                      (unsigned long long)next_version);

    for (edit_log_entry *e = core->pending_head; e; e = e->next) {
        total += snprintf(NULL, 0,
                          "EDIT %s %s %s\n",
                          e->user,
                          e->command,
                          command_result_to_string(e->result));
    }

    total += strlen("END\n");
    return total;
}

char *server_core_commit(server_core *core) {
    if (!core) {
        return NULL;
    }

    uint64_t next_version = core->current_version;

    if (core->pending_count > 0) {
        markdown_increment_version(core->doc);
        core->current_version = core->doc->current_version;
        next_version = core->current_version;
    }

    size_t size = broadcast_payload_size(core, next_version);
    char *payload = malloc(size + 1);
    if (!payload) {
        return NULL;
    }

    size_t off = 0;

    off += snprintf(payload + off,
                    size + 1 - off,
                    "VERSION %llu\n",
                    (unsigned long long)next_version);

    for (edit_log_entry *e = core->pending_head; e; e = e->next) {
        off += snprintf(payload + off,
                        size + 1 - off,
                        "EDIT %s %s %s\n",
                        e->user,
                        e->command,
                        command_result_to_string(e->result));
    }

    snprintf(payload + off, size + 1 - off, "END\n");

    free_log_entries(core->pending_head);
    core->pending_head = NULL;
    core->pending_tail = NULL;
    core->pending_count = 0;

    return payload;
}

char *server_core_flatten_doc(server_core *core) {
    if (!core) {
        return NULL;
    }

    return markdown_flatten(core->doc);
}