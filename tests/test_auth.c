#include "auth.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ROLES_PATH "test_roles.txt"

static void write_test_roles(const char *content) {
    FILE *fp = fopen(TEST_ROLES_PATH, "w");
    assert(fp != NULL);

    fputs(content, fp);
    fclose(fp);
}

static void remove_test_roles(void) {
    remove(TEST_ROLES_PATH);
}

static void test_lookup_write_role(void) {
    write_test_roles(
        "bob write\n"
        "eve read\n"
    );

    user_role role = ROLE_READ;

    assert(auth_lookup_role(TEST_ROLES_PATH, "bob", &role) == AUTH_OK);
    assert(role == ROLE_WRITE);

    remove_test_roles();
}

static void test_lookup_read_role(void) {
    write_test_roles(
        "bob write\n"
        "eve read\n"
    );

    user_role role = ROLE_WRITE;

    assert(auth_lookup_role(TEST_ROLES_PATH, "eve", &role) == AUTH_OK);
    assert(role == ROLE_READ);

    remove_test_roles();
}

static void test_lookup_case_sensitive_username(void) {
    write_test_roles(
        "bob write\n"
        "Bob read\n"
    );

    user_role role = ROLE_READ;

    assert(auth_lookup_role(TEST_ROLES_PATH, "bob", &role) == AUTH_OK);
    assert(role == ROLE_WRITE);

    assert(auth_lookup_role(TEST_ROLES_PATH, "Bob", &role) == AUTH_OK);
    assert(role == ROLE_READ);

    assert(auth_lookup_role(TEST_ROLES_PATH, "BOB", &role) == AUTH_NOT_FOUND);

    remove_test_roles();
}

static void test_ignores_leading_and_trailing_whitespace(void) {
    write_test_roles(
        "   bob     write   \n"
        "\teve\tread\t\n"
    );

    user_role role = ROLE_READ;

    assert(auth_lookup_role(TEST_ROLES_PATH, "bob", &role) == AUTH_OK);
    assert(role == ROLE_WRITE);

    assert(auth_lookup_role(TEST_ROLES_PATH, "eve", &role) == AUTH_OK);
    assert(role == ROLE_READ);

    remove_test_roles();
}

static void test_not_found(void) {
    write_test_roles(
        "bob write\n"
        "eve read\n"
    );

    user_role role = ROLE_READ;

    assert(auth_lookup_role(TEST_ROLES_PATH, "alice", &role) == AUTH_NOT_FOUND);

    remove_test_roles();
}

static void test_invalid_role_for_matching_user(void) {
    write_test_roles(
        "bob admin\n"
        "eve read\n"
    );

    user_role role = ROLE_READ;

    assert(auth_lookup_role(TEST_ROLES_PATH, "bob", &role) == AUTH_INVALID_ROLE);

    remove_test_roles();
}

static void test_invalid_role_for_non_matching_user_is_ignored(void) {
    write_test_roles(
        "mallory admin\n"
        "eve read\n"
    );

    user_role role = ROLE_WRITE;

    assert(auth_lookup_role(TEST_ROLES_PATH, "eve", &role) == AUTH_OK);
    assert(role == ROLE_READ);

    remove_test_roles();
}

static void test_missing_file(void) {
    remove_test_roles();

    user_role role = ROLE_READ;

    assert(auth_lookup_role(TEST_ROLES_PATH, "bob", &role) == AUTH_IO_ERROR);
}

static void test_result_to_string(void) {
    assert(strcmp(auth_result_to_string(AUTH_OK), "AUTH_OK") == 0);
    assert(strcmp(auth_result_to_string(AUTH_NOT_FOUND), "AUTH_NOT_FOUND") == 0);
    assert(strcmp(auth_result_to_string(AUTH_INVALID_ROLE), "AUTH_INVALID_ROLE") == 0);
    assert(strcmp(auth_result_to_string(AUTH_IO_ERROR), "AUTH_IO_ERROR") == 0);
}

int main(void) {
    test_lookup_write_role();
    test_lookup_read_role();
    test_lookup_case_sensitive_username();
    test_ignores_leading_and_trailing_whitespace();
    test_not_found();
    test_invalid_role_for_matching_user();
    test_invalid_role_for_non_matching_user_is_ignored();
    test_missing_file();
    test_result_to_string();

    printf("All auth tests passed.\n");
    return 0;
}