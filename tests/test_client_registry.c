#include "client_registry.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_tmpfile_contents(FILE *fp) {
    assert(fp != NULL);

    fflush(fp);
    rewind(fp);

    char buffer[1024];
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[n] = '\0';

    char *out = malloc(n + 1);
    assert(out != NULL);

    memcpy(out, buffer, n + 1);
    return out;
}

static void test_init_empty_registry(void) {
    client_registry *registry = client_registry_init();

    assert(registry != NULL);
    assert(client_registry_count(registry) == 0);
    assert(client_registry_find(registry, 123) == NULL);

    client_registry_free(registry);
}

static void test_add_and_find_client(void) {
    client_registry *registry = client_registry_init();
    FILE *out = tmpfile();
    assert(out != NULL);

    assert(client_registry_add(registry,
                               1001,
                               "bob",
                               ROLE_WRITE,
                               "FIFO_C2S_1001",
                               "FIFO_S2C_1001",
                               out) == 0);

    assert(client_registry_count(registry) == 1);

    client_conn *client = client_registry_find(registry, 1001);
    assert(client != NULL);
    assert(client->pid == 1001);
    assert(strcmp(client->username, "bob") == 0);
    assert(client->role == ROLE_WRITE);
    assert(strcmp(client->fifo_c2s_path, "FIFO_C2S_1001") == 0);
    assert(strcmp(client->fifo_s2c_path, "FIFO_S2C_1001") == 0);

    client_registry_free(registry);
}

static void test_reject_duplicate_pid(void) {
    client_registry *registry = client_registry_init();

    FILE *out1 = tmpfile();
    FILE *out2 = tmpfile();

    assert(out1 != NULL);
    assert(out2 != NULL);

    assert(client_registry_add(registry,
                               1001,
                               "bob",
                               ROLE_WRITE,
                               "FIFO_C2S_1001",
                               "FIFO_S2C_1001",
                               out1) == 0);

    assert(client_registry_add(registry,
                               1001,
                               "alice",
                               ROLE_READ,
                               "FIFO_C2S_1001_DUP",
                               "FIFO_S2C_1001_DUP",
                               out2) == -1);

    /*
     * Since duplicate add fails, registry does not own out2.
     */
    fclose(out2);

    assert(client_registry_count(registry) == 1);

    client_registry_free(registry);
}

static void test_remove_client(void) {
    client_registry *registry = client_registry_init();

    FILE *out1 = tmpfile();
    FILE *out2 = tmpfile();

    assert(out1 != NULL);
    assert(out2 != NULL);

    assert(client_registry_add(registry,
                               1001,
                               "bob",
                               ROLE_WRITE,
                               "FIFO_C2S_1001",
                               "FIFO_S2C_1001",
                               out1) == 0);

    assert(client_registry_add(registry,
                               1002,
                               "eve",
                               ROLE_READ,
                               "FIFO_C2S_1002",
                               "FIFO_S2C_1002",
                               out2) == 0);

    assert(client_registry_count(registry) == 2);

    assert(client_registry_remove(registry, 1001) == 0);
    assert(client_registry_count(registry) == 1);
    assert(client_registry_find(registry, 1001) == NULL);
    assert(client_registry_find(registry, 1002) != NULL);

    assert(client_registry_remove(registry, 9999) == -1);
    assert(client_registry_count(registry) == 1);

    client_registry_free(registry);
}

static void test_broadcast_single_client(void) {
    client_registry *registry = client_registry_init();

    FILE *out = tmpfile();
    assert(out != NULL);

    assert(client_registry_add(registry,
                               1001,
                               "bob",
                               ROLE_WRITE,
                               "FIFO_C2S_1001",
                               "FIFO_S2C_1001",
                               out) == 0);

    client_conn *client = client_registry_find(registry, 1001);
    assert(client != NULL);

    assert(client_registry_broadcast(registry,
                                     "VERSION 1\n"
                                     "EDIT bob INSERT 0 Hello SUCCESS\n"
                                     "END\n") == 0);

    char *contents = read_tmpfile_contents(client->out_stream);

    assert(strcmp(contents,
                  "VERSION 1\n"
                  "EDIT bob INSERT 0 Hello SUCCESS\n"
                  "END\n") == 0);

    free(contents);
    client_registry_free(registry);
}

static void test_broadcast_multiple_clients(void) {
    client_registry *registry = client_registry_init();

    FILE *out1 = tmpfile();
    FILE *out2 = tmpfile();

    assert(out1 != NULL);
    assert(out2 != NULL);

    assert(client_registry_add(registry,
                               1001,
                               "bob",
                               ROLE_WRITE,
                               "FIFO_C2S_1001",
                               "FIFO_S2C_1001",
                               out1) == 0);

    assert(client_registry_add(registry,
                               1002,
                               "eve",
                               ROLE_READ,
                               "FIFO_C2S_1002",
                               "FIFO_S2C_1002",
                               out2) == 0);

    client_conn *bob = client_registry_find(registry, 1001);
    client_conn *eve = client_registry_find(registry, 1002);

    assert(bob != NULL);
    assert(eve != NULL);

    const char *payload =
        "VERSION 1\n"
        "EDIT bob INSERT 0 Hello SUCCESS\n"
        "END\n";

    assert(client_registry_broadcast(registry, payload) == 0);

    char *bob_contents = read_tmpfile_contents(bob->out_stream);
    char *eve_contents = read_tmpfile_contents(eve->out_stream);

    assert(strcmp(bob_contents, payload) == 0);
    assert(strcmp(eve_contents, payload) == 0);

    free(bob_contents);
    free(eve_contents);

    client_registry_free(registry);
}

int main(void) {
    test_init_empty_registry();
    test_add_and_find_client();
    test_reject_duplicate_pid();
    test_remove_client();
    test_broadcast_single_client();
    test_broadcast_multiple_clients();

    printf("All client registry tests passed.\n");
    return 0;
}