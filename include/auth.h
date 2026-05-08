#ifndef AUTH_H
#define AUTH_H

#include "command.h"

typedef enum {
    AUTH_OK = 0,
    AUTH_NOT_FOUND = -1,
    AUTH_INVALID_ROLE = -2,
    AUTH_IO_ERROR = -3
} auth_result;

int auth_lookup_role(const char *roles_path,
                     const char *username,
                     user_role *out_role);

const char *auth_result_to_string(int result);

#endif