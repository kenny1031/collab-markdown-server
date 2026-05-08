#include "server_core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_doc(server_core *core, const char *expected) {
    char *actual = server_core_flatten_doc(core);
    assert(actual != NULL);

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "\nExpected doc:\n[%s]\nActual doc:\n[%s]\n",
                expected, actual);
        free(actual);
        assert(0);
    }

    free(actual);
}

static void assert_payload(const char *actual, const char *expected) {
    assert(actual != NULL);

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "\nExpected payload:\n[%s]\nActual payload:\n[%s]\n",
                expected, actual);
        assert(0);
    }
}

static void test_single_insert_commit(void) {
    server_core *core = server_core_init();

    assert(server_core_submit(core, "bob", ROLE_WRITE, "INSERT 0 Hello\n") == CMD_SUCCESS);

    assert_doc(core, "");

    char *payload = server_core_commit(core);

    assert_payload(payload,
                   "VERSION 1\n"
                   "EDIT bob INSERT 0 Hello SUCCESS\n"
                   "END\n");

    assert_doc(core, "Hello");

    free(payload);
    server_core_free(core);
}

static void test_multiple_commands_same_commit(void) {
    server_core *core = server_core_init();

    assert(server_core_submit(core, "bob", ROLE_WRITE, "INSERT 0 World\n") == CMD_SUCCESS);
    assert(server_core_submit(core, "alice", ROLE_WRITE, "INSERT 0 Hello \n") == CMD_SUCCESS);

    char *payload = server_core_commit(core);

    assert_payload(payload,
                   "VERSION 1\n"
                   "EDIT bob INSERT 0 World SUCCESS\n"
                   "EDIT alice INSERT 0 Hello  SUCCESS\n"
                   "END\n");

    assert_doc(core, "Hello World");

    free(payload);
    server_core_free(core);
}

static void test_read_role_rejected_but_logged(void) {
    server_core *core = server_core_init();

    assert(server_core_submit(core, "eve", ROLE_READ, "INSERT 0 Hello\n") ==
           CMD_REJECT_UNAUTHORISED);

    char *payload = server_core_commit(core);

    assert_payload(payload,
                   "VERSION 1\n"
                   "EDIT eve INSERT 0 Hello Reject UNAUTHORISED\n"
                   "END\n");

    assert_doc(core, "");

    free(payload);
    server_core_free(core);
}

static void test_invalid_position_rejected_but_logged(void) {
    server_core *core = server_core_init();

    assert(server_core_submit(core, "bob", ROLE_WRITE, "INSERT 99 Hello\n") ==
           CMD_REJECT_INVALID_POSITION);

    char *payload = server_core_commit(core);

    assert_payload(payload,
                   "VERSION 1\n"
                   "EDIT bob INSERT 99 Hello Reject INVALID_POSITION\n"
                   "END\n");

    assert_doc(core, "");

    free(payload);
    server_core_free(core);
}

static void test_empty_commit_keeps_version_zero(void) {
    server_core *core = server_core_init();

    char *payload = server_core_commit(core);

    assert_payload(payload,
                   "VERSION 0\n"
                   "END\n");

    assert_doc(core, "");

    free(payload);
    server_core_free(core);
}

static void test_two_commits_increment_versions(void) {
    server_core *core = server_core_init();

    assert(server_core_submit(core, "bob", ROLE_WRITE, "INSERT 0 Hello\n") == CMD_SUCCESS);
    char *payload1 = server_core_commit(core);

    assert_payload(payload1,
                   "VERSION 1\n"
                   "EDIT bob INSERT 0 Hello SUCCESS\n"
                   "END\n");

    free(payload1);

    assert(server_core_submit(core, "bob", ROLE_WRITE, "INSERT 5  World\n") == CMD_SUCCESS);
    char *payload2 = server_core_commit(core);

    assert_payload(payload2,
                   "VERSION 2\n"
                   "EDIT bob INSERT 5  World SUCCESS\n"
                   "END\n");

    assert_doc(core, "Hello World");

    free(payload2);
    server_core_free(core);
}

static void test_formatting_command_commit(void) {
    server_core *core = server_core_init();

    assert(server_core_submit(core, "bob", ROLE_WRITE, "INSERT 0 hello\n") == CMD_SUCCESS);
    char *payload1 = server_core_commit(core);
    free(payload1);

    assert(server_core_submit(core, "bob", ROLE_WRITE, "BOLD 0 5\n") == CMD_SUCCESS);
    char *payload2 = server_core_commit(core);

    assert_payload(payload2,
                   "VERSION 2\n"
                   "EDIT bob BOLD 0 5 SUCCESS\n"
                   "END\n");

    assert_doc(core, "**hello**");

    free(payload2);
    server_core_free(core);
}

int main(void) {
    test_single_insert_commit();
    test_multiple_commands_same_commit();
    test_read_role_rejected_but_logged();
    test_invalid_position_rejected_but_logged();
    test_empty_commit_keeps_version_zero();
    test_two_commits_increment_versions();
    test_formatting_command_commit();

    printf("All server core tests passed.\n");
    return 0;
}