#ifndef RV64I_H
#define RV64I_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RV_REGISTER,
    RV_IMMEDIATE,
    RV_MEMORY,
    RV_LABEL
} RvOperandKind;

typedef struct {
    RvOperandKind kind;
    union {
        unsigned reg;
        int64_t immediate;
        struct {
            unsigned base;
            int64_t offset;
        } memory;
        const char *label;
    } value;
} RvOperand;

typedef struct {
    uint64_t address;
    size_t line;
    const char *mnemonic;
    size_t operand_count;
    RvOperand operands[3];
} RvInstruction;

typedef struct {
    const char *name;
    uint64_t address;
} RvSymbol;

typedef struct {
    size_t line;
    char message[160];
} RvError;

/* Encode one parsed instruction; leave *word alone if it fails.
 * error can be NULL. symbols can be NULL only when symbol_count is zero.
 * Inputs belong to the caller. See README.md for the operand format. */
bool rv64i_encode(const RvInstruction *instruction,
                  const RvSymbol *symbols, size_t symbol_count,
                  uint32_t *word, RvError *error);

/* Decode one supported canonical word at a four-byte-aligned address.
 * The mnemonic has static storage; operands use register numbers and numeric
 * byte displacements. line is zero because machine code has no source lines.
 * Leave *instruction unchanged on failure. error can be NULL. */
bool rv64i_decode(uint32_t word, uint64_t address,
                  RvInstruction *instruction, RvError *error);

/* Format a validated numeric instruction as canonical, reassemblable text.
 * Uses xN registers, decimal offsets, hexadecimal U fields, and numeric fence
 * masks. Resolve labels first. size includes the terminating NUL byte.
 * Leave text unchanged on failure. error can be NULL. */
bool rv64i_format(const RvInstruction *instruction,
                  char *text, size_t size, RvError *error);

#endif
