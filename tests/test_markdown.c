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

static void test_init_empty_document(void) {
    document *doc = markdown_init();

    assert(doc != NULL);
    assert(doc->current_version == 0);
    assert_doc(doc, "");

    markdown_free(doc);
}

static void test_insert_commit(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "World") == SUCCESS);

    // Pending edits are not visible before commit.
    assert_doc(doc, "");

    markdown_increment_version(doc);

    assert(doc->current_version == 1);
    assert_doc(doc, "World");

    markdown_free(doc);
}

static void test_multiple_pending_insertions_same_version(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "World") == SUCCESS);
    assert(markdown_insert(doc, 0, 0, "Hello ") == SUCCESS);

    assert_doc(doc, "");

    markdown_increment_version(doc);

    assert_doc(doc, "Hello World");

    markdown_free(doc);
}

static void test_insert_invalid_position(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 1, "x") == INVALID_CURSOR_POS);

    markdown_free(doc);
}

static void test_outdated_version_insert(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_insert(doc, 0, 0, "old") == OUTDATED_VERSION);
    assert_doc(doc, "hello");

    markdown_free(doc);
}

static void test_delete_basic(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Hello World") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_delete(doc, 1, 5, 6) == SUCCESS);
    assert_doc(doc, "Hello World");

    markdown_increment_version(doc);

    assert_doc(doc, "Hello");

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

static void test_delete_invalid_position(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "abc") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_delete(doc, 1, 4, 1) == INVALID_CURSOR_POS);
    assert_doc(doc, "abc");

    markdown_free(doc);
}

static void test_delete_zero_length(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "abc") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_delete(doc, 1, 1, 0) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "abc");

    markdown_free(doc);
}

static void test_newline(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "HelloWorld") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_newline(doc, 1, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Hello\nWorld");

    markdown_free(doc);
}

static void test_heading_at_start(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Title") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_heading(doc, 1, 1, 0) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "# Title");

    markdown_free(doc);
}

static void test_heading_auto_newline_before(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "IntroTitle") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_heading(doc, 1, 2, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Intro\n## Title");

    markdown_free(doc);
}

static void test_heading_invalid_level(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Title") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_heading(doc, 1, 0, 0) == INVALID_CURSOR_POS);
    assert(markdown_heading(doc, 1, 4, 0) == INVALID_CURSOR_POS);

    assert_doc(doc, "Title");

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

static void test_italic(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_italic(doc, 1, 0, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "*hello*");

    markdown_free(doc);
}

static void test_code(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "printf") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_code(doc, 1, 0, 6) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "`printf`");

    markdown_free(doc);
}

static void test_invalid_empty_ranges(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_bold(doc, 1, 2, 2) == INVALID_CURSOR_POS);
    assert(markdown_italic(doc, 1, 3, 3) == INVALID_CURSOR_POS);
    assert(markdown_code(doc, 1, 1, 1) == INVALID_CURSOR_POS);
    assert(markdown_link(doc, 1, 0, 0, "https://example.com") == INVALID_CURSOR_POS);

    assert_doc(doc, "hello");

    markdown_free(doc);
}

static void test_invalid_range_end_out_of_bounds(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_bold(doc, 1, 0, 6) == INVALID_CURSOR_POS);
    assert(markdown_italic(doc, 1, 0, 6) == INVALID_CURSOR_POS);
    assert(markdown_code(doc, 1, 0, 6) == INVALID_CURSOR_POS);
    assert(markdown_link(doc, 1, 0, 6, "https://example.com") == INVALID_CURSOR_POS);

    assert_doc(doc, "hello");

    markdown_free(doc);
}

static void test_blockquote_at_start(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "quote") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_blockquote(doc, 1, 0) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "> quote");

    markdown_free(doc);
}

static void test_blockquote_auto_newline_before(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Introquote") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_blockquote(doc, 1, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Intro\n> quote");

    markdown_free(doc);
}

static void test_unordered_list_at_start(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "item") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_unordered_list(doc, 1, 0) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "- item");

    markdown_free(doc);
}

static void test_unordered_list_auto_newline_before(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Introitem") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_unordered_list(doc, 1, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Intro\n- item");

    markdown_free(doc);
}

static void test_horizontal_rule_at_start(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "content") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_horizontal_rule(doc, 1, 0) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "---\ncontent");

    markdown_free(doc);
}

static void test_horizontal_rule_auto_newline_before(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Introcontent") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_horizontal_rule(doc, 1, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Intro\n---\ncontent");

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

static void test_link_null_url(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Google") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_link(doc, 1, 0, 6, NULL) == INVALID_CURSOR_POS);
    assert_doc(doc, "Google");

    markdown_free(doc);
}

static void test_ordered_list_at_start(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "item") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_ordered_list(doc, 1, 0) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "1. item");

    markdown_free(doc);
}

static void test_ordered_list_auto_newline_before(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Introitem") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_ordered_list(doc, 1, 5) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Intro\n1. item");

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

static void test_ordered_list_insert_in_middle(void) {
    document *doc = markdown_init();

    const char *text =
        "1. tomato\n"
        "2. cheese burger\n"
        "3. asparagus";

    assert(markdown_insert(doc, 0, 0, text) == SUCCESS);
    markdown_increment_version(doc);

    size_t pos = strlen("1. tomato\n2. cheese");

    assert(markdown_ordered_list(doc, 1, pos) == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc,
        "1. tomato\n"
        "2. cheese\n"
        "3. burger\n"
        "4. asparagus");

    markdown_free(doc);
}

static void test_flatten_before_commit_after_existing_version(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "Hello") == SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Hello");

    assert(markdown_insert(doc, 1, 5, " World") == SUCCESS);

    // Still old committed version before increment.
    assert_doc(doc, "Hello");

    markdown_increment_version(doc);

    assert_doc(doc, "Hello World");

    markdown_free(doc);
}

static void test_multiple_formatting_operations_same_version(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello world") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_bold(doc, 1, 0, 5) == SUCCESS);
    assert(markdown_italic(doc, 1, 6, 11) == SUCCESS);

    assert_doc(doc, "hello world");

    markdown_increment_version(doc);

    assert_doc(doc, "**hello** *world*");

    markdown_free(doc);
}

static void test_outdated_version_formatting(void) {
    document *doc = markdown_init();

    assert(markdown_insert(doc, 0, 0, "hello") == SUCCESS);
    markdown_increment_version(doc);

    assert(markdown_bold(doc, 0, 0, 5) == OUTDATED_VERSION);
    assert(markdown_heading(doc, 0, 1, 0) == OUTDATED_VERSION);
    assert(markdown_delete(doc, 0, 0, 1) == OUTDATED_VERSION);

    assert_doc(doc, "hello");

    markdown_free(doc);
}

int main(void) {
    test_init_empty_document();

    test_insert_commit();
    test_multiple_pending_insertions_same_version();
    test_insert_invalid_position();
    test_outdated_version_insert();

    test_delete_basic();
    test_delete_truncate();
    test_delete_invalid_position();
    test_delete_zero_length();

    test_newline();

    test_heading_at_start();
    test_heading_auto_newline_before();
    test_heading_invalid_level();
    test_heading_invalid_pos();

    test_bold();
    test_italic();
    test_code();
    test_invalid_empty_ranges();
    test_invalid_range_end_out_of_bounds();

    test_blockquote_at_start();
    test_blockquote_auto_newline_before();

    test_unordered_list_at_start();
    test_unordered_list_auto_newline_before();

    test_horizontal_rule_at_start();
    test_horizontal_rule_auto_newline_before();

    test_link();
    test_link_null_url();

    test_ordered_list_at_start();
    test_ordered_list_auto_newline_before();
    test_ordered_list_renumber_after_delete();
    test_ordered_list_split_by_newline();
    test_ordered_list_insert_in_middle();

    test_flatten_before_commit_after_existing_version();
    test_multiple_formatting_operations_same_version();
    test_outdated_version_formatting();

    printf("All markdown tests passed.\n");
    return 0;
}