#include "auth.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_LINE_BUF_SIZE 512

static char *trim_left(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static void trim_right_in_place(char *s) {
    size_t n = strlen(s);

    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static int parse_role(const char *role_s, user_role *out_role) {
    if (!role_s || !out_role) {
        return 0;
    }

    if (strcmp(role_s, "write") == 0) {
        *out_role = ROLE_WRITE;
        return 1;
    }

    if (strcmp(role_s, "read") == 0) {
        *out_role = ROLE_READ;
        return 1;
    }

    return 0;
}

int auth_lookup_role(const char *roles_path,
                     const char *username,
                     user_role *out_role) {
    if (!roles_path || !username || !out_role) {
        return AUTH_IO_ERROR;
    }

    FILE *fp = fopen(roles_path, "r");
    if (!fp) {
        return AUTH_IO_ERROR;
    }

    char line[AUTH_LINE_BUF_SIZE];

    while (fgets(line, sizeof(line), fp)) {
        trim_right_in_place(line);
        char *p = trim_left(line);

        if (*p == '\0') {
            continue;
        }

        /*
         * Optional: allow comments in our project version.
         * If you want to match the assignment strictly, remove this block.
         */
        if (*p == '#') {
            continue;
        }

        char *username_s = strtok(p, " \t");
        char *role_s = strtok(NULL, " \t");
        char *extra = strtok(NULL, " \t");

        if (!username_s || !role_s || extra) {
            continue;
        }

        if (strcmp(username_s, username) != 0) {
            continue;
        }

        user_role role;
        if (!parse_role(role_s, &role)) {
            fclose(fp);
            return AUTH_INVALID_ROLE;
        }

        *out_role = role;
        fclose(fp);
        return AUTH_OK;
    }

    fclose(fp);
    return AUTH_NOT_FOUND;
}

const char *auth_result_to_string(int result) {
    switch (result) {
    case AUTH_OK:
        return "AUTH_OK";
    case AUTH_NOT_FOUND:
        return "AUTH_NOT_FOUND";
    case AUTH_INVALID_ROLE:
        return "AUTH_INVALID_ROLE";
    case AUTH_IO_ERROR:
    default:
        return "AUTH_IO_ERROR";
    }
}