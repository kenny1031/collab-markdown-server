#include "markdown.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_doc(document *doc, const char *expected) {
    char *actual = markdown_flatten(doc);
    assert(actual != NULL);

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "\nExpected:\n[%s]\nActual:\n[%s]\n", expected, actual);
        free(actual);
        assert(0);
    }

    free(actual);
}

static void test_insert_commit(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "World") == SUCCESS);
    assert_doc(doc, "");

    markdown_increment_version(doc);
    assert_doc(doc, "World");

    markdown_free(doc);
}

static void test_delete_truncate(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Hello World") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_delete(doc, 1, 5, 100) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Hello");

    markdown_free(doc);
}

static void test_invalid_empty_range(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_bold(doc, 1, 2, 2) == INVALID_CURSOR_POS);
    assert(markdown_italic(doc, 1, 3, 3) == INVALID_CURSOR_POS);
    assert(markdown_code(doc, 1, 1, 1) == INVALID_CURSOR_POS);
    assert(markdown_link(doc, 1, 0, 0, "https://example.com") == INVALID_CURSOR_POS);

    markdown_free(doc);
}

static void test_heading_invalid_pos(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "abc") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_heading(doc, 1, 1, 999) == INVALID_CURSOR_POS);

    markdown_free(doc);
}

static void test_bold(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_bold(doc, 1, 0, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "**hello**");

    markdown_free(doc);
}

static void test_link(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Google") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_link(doc, 1, 0, 6, "https://google.com") == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "[Google](https://google.com)");

    markdown_free(doc);
}

static void test_ordered_list_renumber_after_delete(void) {
    document *doc = markdown_init();

    const char *text =
        "Things to buy\n"
        "1. yoyo\n"
        "2. switch\n"
        "3. ps5\n"
        "4. textbook\n";

    assert(markdown_insert(doc, 0, 0, text) == SUCCESS);
    markdown_increment_version(doc);

    // Delete "2. switch\n"
    size_t pos = strlen("Things to buy\n1. yoyo\n");
    size_t len = strlen("2. switch\n");

    assert(markdown_delete(doc, 1, pos, len) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc,
        "Things to buy\n"
        "1. yoyo\n"
        "2. ps5\n"
        "3. textbook\n");

    markdown_free(doc);
}

static void test_ordered_list_split_by_newline(void) {
    document *doc = markdown_init();

    const char *text =
        "1. tomato\n"
        "2. cheese burger\n"
        "3. asparagus";

    assert(markdown_insert(doc, 0, 0, text) == SUCCESS);
    markdown_increment_version(doc);

    size_t pos = strlen("1. tomato\n2. cheese");

    assert(markdown_newline(doc, 1, pos) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc,
        "1. tomato\n"
        "2. cheese\n"
        "burger\n"
        "1. asparagus");

    markdown_free(doc);
}

int main(void) {
    test_insert_commit();
    test_delete_truncate();
    test_invalid_empty_range();
    test_heading_invalid_pos();
    test_bold();
    test_link();
    test_ordered_list_renumber_after_delete();
    test_ordered_list_split_by_newline();

    printf("All markdown tests passed.\n");
    return 0;
}