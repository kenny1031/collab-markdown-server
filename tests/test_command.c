#include "command.h"

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

static void test_insert_command(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Hello\n") == CMD_SUCCESS);
    assert_doc(doc, "");

    markdown_increment_version(doc);

    assert_doc(doc, "Hello");

    markdown_free(doc);
}

static void test_insert_with_spaces_content(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Hello world from parser\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Hello world from parser");

    markdown_free(doc);
}

static void test_read_role_rejects_write_command(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_READ, "INSERT 0 Hello\n") == CMD_REJECT_UNAUTHORISED);
    markdown_increment_version(doc);

    assert_doc(doc, "");

    markdown_free(doc);
}

static void test_delete_command(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Hello World\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert(command_execute(doc, 1, ROLE_WRITE, "DEL 5 6\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "Hello");

    markdown_free(doc);
}

static void test_bold_command(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 hello\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert(command_execute(doc, 1, ROLE_WRITE, "BOLD 0 5\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "**hello**");

    markdown_free(doc);
}

static void test_heading_command(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Title\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert(command_execute(doc, 1, ROLE_WRITE, "HEADING 2 0\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "## Title");

    markdown_free(doc);
}

static void test_link_command(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Google\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert(command_execute(doc, 1, ROLE_WRITE, "LINK 0 6 https://google.com\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert_doc(doc, "[Google](https://google.com)");

    markdown_free(doc);
}

static void test_invalid_position_mapping(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 99 Hello\n") == CMD_REJECT_INVALID_POSITION);

    markdown_free(doc);
}

static void test_outdated_version_mapping(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Hello\n") == CMD_SUCCESS);
    markdown_increment_version(doc);

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT 0 Old\n") == CMD_REJECT_OUTDATED_VERSION);

    assert_doc(doc, "Hello");

    markdown_free(doc);
}

static void test_invalid_command_missing_args(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "INSERT\n") == CMD_REJECT_INVALID_COMMAND);
    assert(command_execute(doc, 0, ROLE_WRITE, "DEL 0\n") == CMD_REJECT_INVALID_COMMAND);
    assert(command_execute(doc, 0, ROLE_WRITE, "BOLD 0\n") == CMD_REJECT_INVALID_COMMAND);

    markdown_free(doc);
}

static void test_invalid_command_extra_args(void) {
    document *doc = markdown_init();

    assert(command_execute(doc, 0, ROLE_WRITE, "DEL 0 1 extra\n") == CMD_REJECT_INVALID_COMMAND);
    assert(command_execute(doc, 0, ROLE_WRITE, "NEWLINE 0 extra\n") == CMD_REJECT_INVALID_COMMAND);

    markdown_free(doc);
}

static void test_result_to_string(void) {
    assert(strcmp(command_result_to_string(CMD_SUCCESS), "SUCCESS") == 0);
    assert(strcmp(command_result_to_string(CMD_REJECT_UNAUTHORISED), "Reject UNAUTHORISED") == 0);
    assert(strcmp(command_result_to_string(CMD_REJECT_INVALID_POSITION), "Reject INVALID_POSITION") == 0);
    assert(strcmp(command_result_to_string(CMD_REJECT_OUTDATED_VERSION), "Reject OUTDATED_VERSION") == 0);
}

int main(void) {
    test_insert_command();
    test_insert_with_spaces_content();
    test_read_role_rejects_write_command();
    test_delete_command();
    test_bold_command();
    test_heading_command();
    test_link_command();

    test_invalid_position_mapping();
    test_outdated_version_mapping();
    test_invalid_command_missing_args();
    test_invalid_command_extra_args();
    test_result_to_string();

    printf("All command parser tests passed.\n");
    return 0;
}