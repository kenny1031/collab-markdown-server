# Concurrent Markdown Collaboration Server

A POSIX-based collaborative Markdown editing system implemented in C.

The project simulates a lightweight Google Docs-style architecture: a central server maintains the authoritative document state, accepts client edit commands through FIFO-based IPC, applies versioned Markdown operations, and broadcasts committed updates to connected clients.

## Features

- Multi-client server architecture in C
- POSIX FIFO / named pipe communication
- Signal-based client-server connection handshake
- Role-based permissions using `config/roles.txt`
- Markdown command parser and dispatcher
- Versioned document engine with commit-based updates
- Broadcast payloads for client synchronisation
- Modular unit-tested components
- AddressSanitizer-compatible test targets

## Architecture

```text
                 ┌────────────────────┐
                 │      Client A      │
                 │ stdin + local view │
                 └─────────┬──────────┘
                           │
                           │ FIFO_C2S_<pid>
                           │ FIFO_S2C_<pid>
                           ▼
┌────────────────────────────────────────────────────┐
│                    Server Process                  │
│                                                    │
│  ┌────────────────┐    ┌────────────────────────┐  │
│  │ Client Thread A│    │ Client Thread B        │  │
│  │ reads commands │    │ reads commands         │  │
│  └───────┬────────┘    └────────┬───────────────┘  │
│          │                      │                  │
│          └──────────┬───────────┘                  │
│                     ▼                              │
│        ┌─────────────────────────────┐             │
│        │ Server Core                 │             │
│        │ - execute commands          │             │
│        │ - commit document version   │             │
│        │ - build broadcast payload   │             │
│        └──────────────┬──────────────┘             │
│                       ▼                            │
│        ┌─────────────────────────────┐             │
│        │ Markdown Document Engine    │             │
│        │ - insert/delete             │             │
│        │ - bold/italic/code/link     │             │
│        │ - lists/headings/rules      │             │
│        └─────────────────────────────┘             │
└────────────────────────────────────────────────────┘
                           ▲
                           │
                 ┌─────────┴──────────┐
                 │      Client B      │
                 │ stdin + local view │
                 └────────────────────┘
```

## Project Structure

```text
.
├── include
│   ├── auth.h
│   ├── client_registry.h
│   ├── command.h
│   ├── document.h
│   ├── markdown.h
│   └── server_core.h
├── src
│   ├── auth.c
│   ├── client.c
│   ├── client_demo.c
│   ├── client_registry.c
│   ├── command.c
│   ├── markdown.c
│   ├── server.c
│   ├── server_core.c
│   └── server_demo.c
├── tests
│   ├── test_auth.c
│   ├── test_client_registry.c
│   ├── test_command.c
│   ├── test_markdown.c
│   └── test_server_core.c
├── config
│   └── roles.txt
├── Makefile
└── README.md
```

## Core Modules

### `markdown.c`

Implements the document engine.

Supported operations include:

```text
INSERT
DEL
NEWLINE
HEADING
BOLD
ITALIC
BLOCKQUOTE
ORDERED_LIST
UNORDERED_LIST
CODE
HORIZONTAL_RULE
LINK
```

The document uses a commit-based versioning model. Edits are staged as pending operations and become visible after a version increment.

### `command.c`

Parses user command strings and dispatches them to the Markdown engine.

Example commands:

```text
INSERT 0 Hello
BOLD 0 5
ITALIC 6 11
DEL 0 3
LINK 0 6 https://example.com
```

### `server_core.c`

Handles server-side application logic:

```text
submit command
execute command
record edit result
commit document version
build broadcast payload
```

Example broadcast:

```text
VERSION 1
EDIT daniel INSERT 0 Hello SUCCESS
END
```

### `auth.c`

Loads user permissions from `config/roles.txt`.

Example:

```text
ryan read
yao read
daniel write
```

Only `write` users can modify the document. `read` users can connect and receive broadcasts but their edit commands are rejected.

### `client_registry.c`

Tracks connected clients and broadcasts payloads to their output streams.

## Build

Build the formal server and client:

```bash
make app
```

This creates:

```text
server
client
```

Build the FIFO demo version:

```bash
make demo
```

Run all tests:

```bash
make test
```

Clean generated files:

```bash
make clean
```

## Running the Server and Client

First, make sure `config/roles.txt` exists:

```text
daniel write
ryan read
yao read
```

Start the server:

```bash
./server 1000
```

Example output:

```text
Server PID: 29525
Using update interval: 1000 ms
```

Start a write client in another terminal:

```bash
./client 29525 daniel
```

Example output:

```text
Connected as daniel
Role: write
Version: 0
Document length: 0
Document:

Type commands, e.g.:
  INSERT 0 Hello
  BOLD 0 5
  DEL 0 2
  DISCONNECT
```

Now enter:

```text
INSERT 0 Hello
```

Expected broadcast:

```text
[server] VERSION 1
[server] EDIT daniel INSERT 0 Hello SUCCESS
[server] END
```

Then:

```text
BOLD 0 5
```

Expected broadcast:

```text
[server] VERSION 2
[server] EDIT daniel BOLD 0 5 SUCCESS
[server] END
```

Start a read-only client:

```bash
./client 29525 ryan
```

If `ryan` tries to edit:

```text
INSERT 0 Bad
```

The server rejects the command:

```text
[server] EDIT ryan INSERT 0 Bad Reject UNAUTHORISED
```

Disconnect:

```text
DISCONNECT
```

## Local FIFO Demo

The project also includes a simplified single-client FIFO demo.

Terminal 1:

```bash
make demo
./server_demo
```

Terminal 2:

```bash
./client_demo
```

Example commands:

```text
INSERT 0 Hello
BOLD 0 5
QUIT
```

This demo is useful for testing FIFO communication without the full signal-based connection flow.

## Testing

Run:

```bash
make test
```

Test coverage includes:

```text
Markdown document operations
Command parsing and dispatch
Server commit pipeline
Role-based authentication
Client registry and broadcast behavior
```

The tests are compiled with:

```text
-Wall -Wextra -Werror
-fsanitize=address,undefined
```

On macOS, AddressSanitizer may print:

```text
malloc: nano zone abandoned due to inability to reserve vm space
```

This is a common macOS sanitizer message and does not indicate a test failure.

## Platform Notes

On Linux, the formal server can use realtime signals for client-server connection handshakes.

On macOS, the local development build falls back to `SIGUSR1` and `SIGUSR2`, because macOS does not expose `SIGRTMIN`.