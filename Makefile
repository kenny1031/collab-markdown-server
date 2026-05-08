CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -Iinclude
SANFLAGS = -fsanitize=address,undefined

MARKDOWN_SRC = src/markdown.c
COMMAND_SRC = src/command.c
SERVER_CORE_SRC = src/server_core.c

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
	$(CC) $(CFLAGS) $(MARKDOWN_SRC) $(COMMAND_SRC) $(SERVER_CORE_SRC) tests/test_server_core.c -o test_server_core
	./test_server_core

test: test-markdown test-command test-server-core

clean:
	rm -f test_markdown test_command test_server_core server client *.o src/*.o tests/*.o doc.md FIFO_*