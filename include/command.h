#ifndef COMMAND_H
#define COMMAND_H
#include <stdint.h>
#include <stddef.h>
#include "markdown.h"

typedef enum {
    ROLE_READ = 0,
    ROLE_WRITE = 1
} user_role;

typedef enum {
    CMD_SUCCESS = 0,
    CMD_REJECT_UNAUTHORISED = -10,
    CMD_REJECT_INVALID_POSITION = -11,
    CMD_REJECT_DELETED_POSITION = -12,
    CMD_REJECT_OUTDATED_VERSION = -13,
    CMD_REJECT_INVALID_COMMAND = -14
} command_result;

int command_execute(document *doc,
                    uint64_t version,
                    user_role role,
                    const char *line);

const char *command_result_to_string(int result);
#endif //COMMAND_H
