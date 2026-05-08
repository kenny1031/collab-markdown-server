#include "command.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMAND_LEN 256

static int is_write_command(const char *cmd) {
    return strcmp(cmd, "INSERT") == 0 ||
           strcmp(cmd, "DEL") == 0 ||
           strcmp(cmd, "NEWLINE") == 0 ||
           strcmp(cmd, "HEADING") == 0 ||
           strcmp(cmd, "BOLD") == 0 ||
           strcmp(cmd, "ITALIC") == 0 ||
           strcmp(cmd, "BLOCKQUOTE") == 0 ||
           strcmp(cmd, "ORDERED_LIST") == 0 ||
           strcmp(cmd, "UNORDERED_LIST") == 0 ||
           strcmp(cmd, "CODE") == 0 ||
           strcmp(cmd, "HORIZONTAL_RULE") == 0 ||
           strcmp(cmd, "LINK") == 0;
}

static int map_markdown_result(int result) {
    if (result == SUCCESS) {
        return CMD_SUCCESS;
    }

    if (result == INVALID_CURSOR_POS) {
        return CMD_REJECT_INVALID_POSITION;
    }

    if (result == DELETED_POSITION) {
        return CMD_REJECT_DELETED_POSITION;
    }

    if (result == OUTDATED_VERSION) {
        return CMD_REJECT_OUTDATED_VERSION;
    }

    return CMD_REJECT_INVALID_COMMAND;
}

static int validate_raw_command_line(const char *line) {
    if (!line) {
        return 0;
    }

    size_t len = strlen(line);

    if (len == 0 || len > MAX_COMMAND_LEN) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)line[i];

        if (c == '\n') {
            if (i != len - 1) {
                return 0;
            }
            continue;
        }

        if (c < 32 || c > 126) {
            return 0;
        }
    }

    return 1;
}

static char *copy_without_trailing_newline(const char *line) {
    size_t len = strlen(line);

    if (len > 0 && line[len - 1] == '\n') {
        len--;
    }

    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, line, len);
    copy[len] = '\0';

    return copy;
}

static int parse_size(const char *s, size_t *out) {
    if (!s || !*s || !out) {
        return 0;
    }

    char *end = NULL;
    unsigned long value = strtoul(s, &end, 10);

    if (*end != '\0') {
        return 0;
    }

    *out = (size_t)value;
    return 1;
}

int command_execute(document *doc,
                    uint64_t version,
                    user_role role,
                    const char *line) {
    if (!doc || !validate_raw_command_line(line)) {
        return CMD_REJECT_INVALID_COMMAND;
    }

    char *buf = copy_without_trailing_newline(line);
    if (!buf) {
        return CMD_REJECT_INVALID_COMMAND;
    }

    char *save = NULL;
    char *cmd = strtok_r(buf, " ", &save);

    if (!cmd) {
        free(buf);
        return CMD_REJECT_INVALID_COMMAND;
    }

    if (is_write_command(cmd) && role != ROLE_WRITE) {
        free(buf);
        return CMD_REJECT_UNAUTHORISED;
    }

    int result = CMD_REJECT_INVALID_COMMAND;

    if (strcmp(cmd, "INSERT") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *content = save;

        if (!pos_s || !content || content[0] == '\0') {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;

            if (!parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_insert(doc, version, pos, content)
                );
            }
        }
    } else if (strcmp(cmd, "DEL") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *len_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!pos_s || !len_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;
            size_t len = 0;

            if (!parse_size(pos_s, &pos) || !parse_size(len_s, &len)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_delete(doc, version, pos, len)
                );
            }
        }
    } else if (strcmp(cmd, "NEWLINE") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!pos_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;

            if (!parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_newline(doc, version, pos)
                );
            }
        }
    } else if (strcmp(cmd, "HEADING") == 0) {
        char *level_s = strtok_r(NULL, " ", &save);
        char *pos_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!level_s || !pos_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t level = 0;
            size_t pos = 0;

            if (!parse_size(level_s, &level) || !parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_heading(doc, version, level, pos)
                );
            }
        }
    } else if (strcmp(cmd, "BOLD") == 0) {
        char *start_s = strtok_r(NULL, " ", &save);
        char *end_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!start_s || !end_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t start = 0;
            size_t end = 0;

            if (!parse_size(start_s, &start) || !parse_size(end_s, &end)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_bold(doc, version, start, end)
                );
            }
        }
    } else if (strcmp(cmd, "ITALIC") == 0) {
        char *start_s = strtok_r(NULL, " ", &save);
        char *end_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!start_s || !end_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t start = 0;
            size_t end = 0;

            if (!parse_size(start_s, &start) || !parse_size(end_s, &end)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_italic(doc, version, start, end)
                );
            }
        }
    } else if (strcmp(cmd, "BLOCKQUOTE") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!pos_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;

            if (!parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_blockquote(doc, version, pos)
                );
            }
        }
    } else if (strcmp(cmd, "ORDERED_LIST") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!pos_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;

            if (!parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_ordered_list(doc, version, pos)
                );
            }
        }
    } else if (strcmp(cmd, "UNORDERED_LIST") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!pos_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;

            if (!parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_unordered_list(doc, version, pos)
                );
            }
        }
    } else if (strcmp(cmd, "CODE") == 0) {
        char *start_s = strtok_r(NULL, " ", &save);
        char *end_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!start_s || !end_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t start = 0;
            size_t end = 0;

            if (!parse_size(start_s, &start) || !parse_size(end_s, &end)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_code(doc, version, start, end)
                );
            }
        }
    } else if (strcmp(cmd, "HORIZONTAL_RULE") == 0) {
        char *pos_s = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!pos_s || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t pos = 0;

            if (!parse_size(pos_s, &pos)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_horizontal_rule(doc, version, pos)
                );
            }
        }
    } else if (strcmp(cmd, "LINK") == 0) {
        char *start_s = strtok_r(NULL, " ", &save);
        char *end_s = strtok_r(NULL, " ", &save);
        char *url = strtok_r(NULL, " ", &save);
        char *extra = strtok_r(NULL, " ", &save);

        if (!start_s || !end_s || !url || extra) {
            result = CMD_REJECT_INVALID_COMMAND;
        } else {
            size_t start = 0;
            size_t end = 0;

            if (!parse_size(start_s, &start) || !parse_size(end_s, &end)) {
                result = CMD_REJECT_INVALID_COMMAND;
            } else {
                result = map_markdown_result(
                    markdown_link(doc, version, start, end, url)
                );
            }
        }
    } else {
        result = CMD_REJECT_INVALID_COMMAND;
    }

    free(buf);
    return result;
}

const char *command_result_to_string(int result) {
    switch (result) {
    case CMD_SUCCESS:
        return "SUCCESS";
    case CMD_REJECT_UNAUTHORISED:
        return "Reject UNAUTHORISED";
    case CMD_REJECT_INVALID_POSITION:
        return "Reject INVALID_POSITION";
    case CMD_REJECT_DELETED_POSITION:
        return "Reject DELETED_POSITION";
    case CMD_REJECT_OUTDATED_VERSION:
        return "Reject OUTDATED_VERSION";
    case CMD_REJECT_INVALID_COMMAND:
    default:
        return "Reject INVALID_COMMAND";
    }
}