CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -Iinclude
SANFLAGS = -fsanitize=address,undefined

test-markdown: CFLAGS += $(SANFLAGS)
test-markdown:
	$(CC) $(CFLAGS) src/markdown.c tests/test_markdown.c -o test_markdown
	./test_markdown

clean:
	rm -f test_markdown server client *.o src/*.o tests/*.o doc.md FIFO_*