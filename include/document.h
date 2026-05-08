#ifndef DOCUMENT_H
#define DOCUMENT_H
#include <stdio.h>
#include <stdint.h>
/**
 * This file is the header file for all the document functions. You will be
 * tested on the functions inside markdown.h You are allowed to and encouraged
 * multiple helper functions and data structures, and make your code as modular
 * as possible. Ensure you DO NOT change the name of document struct.
 */

typedef struct chunk {
    char *data;         // Pointer to the chunk's text buffer
    size_t length;      // Length of the text in this chunk
    struct chunk *prev; // Previous chunk in the list
    struct chunk *next; // Next chunk in the list
} chunk;

typedef struct document {
    chunk *committed_head;
    chunk *committed_tail;
    size_t committed_chunks;

    chunk *head;     // First chunk in the document
    chunk *tail;     // Last chunk in the document
    size_t n_chunks; // Number of chunks in the document
    uint64_t current_version;
} document;

// Functions from here onwards.
#endif

