#ifndef PARSER_H
#define PARSER_H

#include "rv64i.h"
#include <stdio.h>

typedef struct {
    RvInstruction *instructions;
    size_t instruction_count;
    RvSymbol *symbols;
    size_t symbol_count;
    /* Owns the strings used by instructions and symbols. */
    char **storage;
    size_t storage_count;
} RvProgram;

/* Start with a zeroed program. On failure it is freed and zeroed again.
 * Source lines may contain at most 4094 bytes, excluding the newline.
 * The caller keeps ownership of input. */
bool rv_parse(FILE *input, uint64_t base, RvProgram *program, RvError *error);
void rv_program_free(RvProgram *program);

#endif
