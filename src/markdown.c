#include "markdown.h"
#include "document.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================================
// Local string helpers
// ================================
static char *md_strdup(const char *s) {
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

static int is_ordered_list_line(const char *line, int *number) {
    if (!line || !isdigit((unsigned char)line[0])) {
        return 0;
    }

    if (line[1] != '.' || line[2] != ' ') {
        return 0;
    }

    if (number) *number = line[0] - '0';

    return 1;
}

// Renumber each consecutive ordered-list block independently.
static char *renumber_ordered_lists(const char *text) {
    if (!text) {
        return NULL;
    }

    size_t in_len = strlen(text);

    // Output will be at most slightly larger than input.
    // Since assignment only requires ordered list numbers 1-9, same digit width.
    char *out = malloc(in_len + 1);
    if (!out) {
        return NULL;
    }

    size_t out_pos = 0;
    size_t i = 0;
    int current_list_no = 1;
    int in_ordered_block = 0;

    while (i < in_len) {
        size_t line_start = i;
        size_t line_end = i;

        while (line_end < in_len && text[line_end] != '\n') {
            line_end++;
        }

        size_t line_len = line_end - line_start;
        const char *line = text + line_start;

        int old_no = 0;
        if (is_ordered_list_line(line, &old_no)) {
            if (!in_ordered_block) {
                current_list_no = 1;
                in_ordered_block = 1;
            }

            // Preserve only 1-digit numbering, as guaranteed by the spec.
            out[out_pos++] = (char)('0' + current_list_no);
            out[out_pos++] = '.';
            out[out_pos++] = ' ';

            if (line_len > 3) {
                memcpy(out + out_pos, line + 3, line_len - 3);
                out_pos += line_len - 3;
            }

            current_list_no++;
            if (current_list_no > 9) {
                current_list_no = 9;
            }
        } else {
            in_ordered_block = 0;

            if (line_len > 0) {
                memcpy(out + out_pos, line, line_len);
                out_pos += line_len;
            }
        }

        if (line_end < in_len && text[line_end] == '\n') {
            out[out_pos++] = '\n';
            i = line_end + 1;
        } else {
            i = line_end;
        }
    }

    out[out_pos] = '\0';
    return out;
}

// ================================
// Pending Operations
// ================================
typedef enum {
    OP_INSERT,
    OP_DELETE
} OpType;

typedef struct Op {
    OpType type;
    size_t pos;
    size_t len;   // Used for delete
    char *data;   // Used for insert
    struct Op *next;
} Op;

typedef struct OpList {
    document *doc;
    Op *head;
    Op *tail;
    struct OpList *next;
} OpList;

static OpList *op_lists = NULL;

static OpList *find_ops(document *doc) {
    for (OpList *p = op_lists; p; p = p->next) {
        if (p->doc == doc) {
            return p;
        }
    }

    return NULL;
}

static OpList *get_or_create_ops(document *doc) {
    OpList *existing = find_ops(doc);
    if (existing) {
        return existing;
    }

    OpList *node = malloc(sizeof(OpList));
    if (!node) {
        return NULL;
    }

    node->doc = doc;
    node->head = NULL;
    node->tail = NULL;
    node->next = op_lists;
    op_lists = node;

    return node;
}

static void clear_ops(document *doc) {
    OpList *lst = find_ops(doc);
    if (!lst) {
        return;
    }

    Op *op = lst->head;
    while (op) {
        Op *next = op->next;
        free(op->data);
        free(op);
        op = next;
    }

    lst->head = NULL;
    lst->tail = NULL;
}

static void free_ops(document *doc) {
    OpList **pp = &op_lists;

    while (*pp) {
        if ((*pp)->doc == doc) {
            OpList *rem = *pp;
            *pp = rem->next;

            Op *op = rem->head;
            while (op) {
                Op *next = op->next;
                free(op->data);
                free(op);
                op = next;
            }

            free(rem);
            return;
        }

        pp = &(*pp)->next;
    }
}

static int enqueue_op(document *doc, Op *op) {
    OpList *lst = get_or_create_ops(doc);
    if (!lst) {
        return -1;
    }

    op->next = NULL;

    if (lst->tail) {
        lst->tail->next = op;
    } else {
        lst->head = op;
    }

    lst->tail = op;
    return SUCCESS;
}

// ================================
// Text materialisation
// ================================

static char *get_committed_text(const document *doc) {
    if (!doc || !doc->committed_head || !doc->committed_head->data) {
        return md_strdup("");
    }

    return md_strdup(doc->committed_head->data);
}

static char *apply_single_op(char *text, const Op *op) {
    if (!text || !op) {
        free(text);
        return NULL;
    }

    size_t old_len = strlen(text);

    if (op->type == OP_INSERT) {
        size_t pos = op->pos;
        if (pos > old_len) {
            pos = old_len;
        }

        size_t data_len = strlen(op->data);
        char *new_text = malloc(old_len + data_len + 1);
        if (!new_text) {
            free(text);
            return NULL;
        }

        memcpy(new_text, text, pos);
        memcpy(new_text + pos, op->data, data_len);
        memcpy(new_text + pos + data_len, text + pos, old_len - pos + 1);

        free(text);
        return new_text;
    }

    if (op->type == OP_DELETE) {
        size_t pos = op->pos;
        size_t len = op->len;

        if (pos > old_len) {
            pos = old_len;
        }

        if (pos + len > old_len) {
            len = old_len - pos;
        }

        char *new_text = malloc(old_len - len + 1);
        if (!new_text) {
            free(text);
            return NULL;
        }

        memcpy(new_text, text, pos);
        memcpy(new_text + pos, text + pos + len, old_len - pos - len + 1);

        free(text);
        return new_text;
    }

    free(text);
    return NULL;
}

static char *get_working_text(const document *doc) {
    char *text = get_committed_text(doc);
    if (!text) {
        return NULL;
    }

    OpList *lst = find_ops((document *)doc);
    if (!lst || !lst->head) {
        return text;
    }

    for (Op *op = lst->head; op; op = op->next) {
        text = apply_single_op(text, op);
        if (!text) {
            return NULL;
        }
    }

    return text;
}

// ================================
// Common validation helpers
// ================================

static int validate_version_and_get_len(document *doc, uint64_t version,
                                        char **working_out, size_t *len_out) {
    if (!doc) {
        return INVALID_CURSOR_POS;
    }

    if (version != doc->current_version) {
        return OUTDATED_VERSION;
    }

    char *working = get_working_text(doc);
    if (!working) {
        return -1;
    }

    if (working_out) {
        *working_out = working;
    } else {
        free(working);
    }

    if (len_out) {
        *len_out = strlen(working);
    }

    return SUCCESS;
}

static int enqueue_insert(document *doc, size_t pos, const char *content) {
    Op *op = malloc(sizeof(Op));
    if (!op) {
        return -1;
    }

    op->type = OP_INSERT;
    op->pos = pos;
    op->len = 0;
    op->data = md_strdup(content ? content : "");

    if (!op->data) {
        free(op);
        return -1;
    }

    int r = enqueue_op(doc, op);
    if (r != SUCCESS) {
        free(op->data);
        free(op);
        return r;
    }

    return SUCCESS;
}

static int enqueue_delete(document *doc, size_t pos, size_t len) {
    Op *op = malloc(sizeof(Op));
    if (!op) {
        return -1;
    }

    op->type = OP_DELETE;
    op->pos = pos;
    op->len = len;
    op->data = NULL;

    int r = enqueue_op(doc, op);
    if (r != SUCCESS) {
        free(op);
        return r;
    }

    return SUCCESS;
}

static int ensure_block_prefix_position(document *doc, uint64_t version,
                                        size_t *pos) {
    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    if (*pos > n) {
        free(working);
        return INVALID_CURSOR_POS;
    }

    if (*pos > 0 && working[*pos - 1] != '\n') {
        free(working);

        r = markdown_insert(doc, version, *pos, "\n");
        if (r != SUCCESS) {
            return r;
        }

        (*pos)++;
        return SUCCESS;
    }

    free(working);
    return SUCCESS;
}

// ================================
// Init and Free
// ================================

document *markdown_init(void) {
    document *doc = calloc(1, sizeof(document));
    if (!doc) {
        return NULL;
    }

    doc->current_version = 0;

    doc->committed_head = malloc(sizeof(chunk));
    if (!doc->committed_head) {
        free(doc);
        return NULL;
    }

    doc->committed_head->data = md_strdup("");
    if (!doc->committed_head->data) {
        free(doc->committed_head);
        free(doc);
        return NULL;
    }

    doc->committed_head->length = 0;
    doc->committed_head->prev = NULL;
    doc->committed_head->next = NULL;

    doc->committed_tail = doc->committed_head;
    doc->committed_chunks = 1;

    doc->head = NULL;
    doc->tail = NULL;
    doc->n_chunks = 0;

    return doc;
}

void markdown_free(document *doc) {
    if (!doc) {
        return;
    }

    chunk *current = doc->committed_head;
    while (current) {
        chunk *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }

    current = doc->head;
    while (current) {
        chunk *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }

    free_ops(doc);
    free(doc);
}

// ================================
// Edit Commands
// ================================

int markdown_insert(document *doc, uint64_t version, size_t pos,
                    const char *content) {
    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    free(working);

    if (pos > n) {
        return INVALID_CURSOR_POS;
    }

    return enqueue_insert(doc, pos, content);
}

int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len) {
    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    free(working);

    if (pos > n) {
        return INVALID_CURSOR_POS;
    }

    if (len == 0) {
        return SUCCESS;
    }

    // DEL is allowed to flow beyond document end.
    // It should truncate at the end instead of being rejected.
    if (pos + len > n) {
        len = n - pos;
    }

    return enqueue_delete(doc, pos, len);
}

// ================================
// Formatting Commands
// ================================

int markdown_newline(document *doc, uint64_t version, size_t pos) {
    return markdown_insert(doc, version, pos, "\n");
}

int markdown_heading(document *doc, uint64_t version, size_t level,
                     size_t pos) {
    if (level < 1 || level > 3) {
        return INVALID_CURSOR_POS;
    }

    int r = ensure_block_prefix_position(doc, version, &pos);
    if (r != SUCCESS) {
        return r;
    }

    char tag[5] = {0};
    memset(tag, '#', level);
    tag[level] = ' ';

    return markdown_insert(doc, version, pos, tag);
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end) {
    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    free(working);

    if (start >= end || end > n) {
        return INVALID_CURSOR_POS;
    }

    r = markdown_insert(doc, version, end, "**");
    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, start, "**");
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end) {
    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    free(working);

    if (start >= end || end > n) {
        return INVALID_CURSOR_POS;
    }

    r = markdown_insert(doc, version, end, "*");
    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, start, "*");
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos) {
    int r = ensure_block_prefix_position(doc, version, &pos);
    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, pos, "> ");
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos) {
    int r = ensure_block_prefix_position(doc, version, &pos);
    if (r != SUCCESS) {
        return r;
    }

    // Insert a placeholder ordered-list prefix.
    // The final numbering is normalised in markdown_increment_version().
    return markdown_insert(doc, version, pos, "1. ");
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos) {
    int r = ensure_block_prefix_position(doc, version, &pos);
    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, pos, "- ");
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end) {
    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    free(working);

    if (start >= end || end > n) {
        return INVALID_CURSOR_POS;
    }

    r = markdown_insert(doc, version, end, "`");
    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, start, "`");
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos) {
    int r = ensure_block_prefix_position(doc, version, &pos);
    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, pos, "---\n");
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end,
                  const char *url) {
    if (!url) {
        return INVALID_CURSOR_POS;
    }

    char *working = NULL;
    size_t n = 0;

    int r = validate_version_and_get_len(doc, version, &working, &n);
    if (r != SUCCESS) {
        return r;
    }

    free(working);

    if (start >= end || end > n) {
        return INVALID_CURSOR_POS;
    }

    size_t url_len = strlen(url);
    size_t suffix_len = url_len + 4; // "](" + url + ")"

    char *suffix = malloc(suffix_len + 1);
    if (!suffix) {
        return -1;
    }

    snprintf(suffix, suffix_len + 1, "](%s)", url);

    r = markdown_insert(doc, version, end, suffix);
    free(suffix);

    if (r != SUCCESS) {
        return r;
    }

    return markdown_insert(doc, version, start, "[");
}

// ================================
// Utilities
// ================================

void markdown_print(const document *doc, FILE *stream) {
    if (!doc || !stream) {
        return;
    }

    char *txt = markdown_flatten(doc);
    if (!txt) {
        return;
    }

    fputs(txt, stream);
    free(txt);
}

char *markdown_flatten(const document *doc) {
    if (!doc) {
        return NULL;
    }

    // Only committed text is visible.
    return get_committed_text(doc);
}

// ================================
// Versioning
// ================================

void markdown_increment_version(document *doc) {
    if (!doc) {
        return;
    }

    char *new_text = get_working_text(doc);
    if (!new_text) {
        return;
    }

    char *renumbered = renumber_ordered_lists(new_text);
    free(new_text);

    if (!renumbered) {
        return;
    }

    if (!doc->committed_head) {
        doc->committed_head = malloc(sizeof(chunk));
        if (!doc->committed_head) {
            free(renumbered);
            return;
        }

        doc->committed_head->prev = NULL;
        doc->committed_head->next = NULL;
        doc->committed_tail = doc->committed_head;
        doc->committed_chunks = 1;
    } else {
        free(doc->committed_head->data);
    }

    doc->committed_head->data = renumbered;
    doc->committed_head->length = strlen(renumbered);

    clear_ops(doc);

    // Version increments after committing pending edits.
    doc->current_version++;
}