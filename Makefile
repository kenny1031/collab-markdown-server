CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -Iinclude
SANFLAGS = -fsanitize=address,undefined

MARKDOWN_SRC = src/markdown.c
COMMAND_SRC = src/command.c
SERVER_CORE_SRC = src/server_core.c
AUTH_SRC = src/auth.c
CLIENT_REGISTRY_SRC = src/client_registry.c

CORE_SRC = $(MARKDOWN_SRC) $(COMMAND_SRC) $(SERVER_CORE_SRC)

test-markdown: CFLAGS += $(SANFLAGS)
test-markdown:
	$(CC) $(CFLAGS) $(MARKDOWN_SRC) tests/test_markdown.c -o test_markdown
	./test_markdown

test-command: CFLAGS += $(SANFLAGS)
test-command:
	$(CC) $(CFLAGS) $(MARKDOWN_SRC) $(COMMAND_SRC) tests/test_command.c -o test_command
	./test_command

test-server-core: CFLAGS += $(SANFLAGS)
test-server-core:
	$(CC) $(CFLAGS) $(CORE_SRC) tests/test_server_core.c -o test_server_core
	./test_server_core

test-auth: CFLAGS += $(SANFLAGS)
test-auth:
	$(CC) $(CFLAGS) $(AUTH_SRC) tests/test_auth.c -o test_auth
	./test_auth

test-client-registry: CFLAGS += $(SANFLAGS)
test-client-registry:
	$(CC) $(CFLAGS) $(CLIENT_REGISTRY_SRC) tests/test_client_registry.c -o test_client_registry
	./test_client_registry

server-demo:
	$(CC) $(CFLAGS) $(CORE_SRC) src/server_demo.c -o server_demo

client-demo:
	$(CC) $(CFLAGS) src/client_demo.c -o client_demo

demo: server-demo client-demo

test: test-markdown test-command test-server-core test-auth test-client-registry

clean:
	rm -f test_markdown test_command test_server_core test_auth test_client_registry server client server_demo client_demo *.o src/*.o tests/*.o doc.md FIFO_* test_roles.txt