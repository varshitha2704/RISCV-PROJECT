#include "rv64i.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define REG(n) { .kind = RV_REGISTER, .value.reg = (n) }
#define IMM(n) { .kind = RV_IMMEDIATE, .value.immediate = (n) }
#define MEM(r, n) { .kind = RV_MEMORY, .value.memory = { (r), (n) } }
#define LABEL(s) { .kind = RV_LABEL, .value.label = (s) }

int main(void)
{
    /* Example instructions in the format the parser will pass in. */
    const struct {
        const char *format;
        const char *assembly;
        RvInstruction instruction;
    } examples[] = {
        {"R", "add s1, s4, s5",
         {.address = 0x1000, .line = 1, .mnemonic = "add", .operand_count = 3,
          .operands = {REG(9), REG(20), REG(21)}}},
        {"I", "addi t0, zero, -101",
         {.address = 0x1004, .line = 2, .mnemonic = "addi", .operand_count = 3,
          .operands = {REG(5), REG(0), IMM(-101)}}},
        {"S", "sw t0, 40(a0)",
         {.address = 0x1008, .line = 3, .mnemonic = "sw", .operand_count = 2,
          .operands = {REG(5), MEM(10, 40)}}},
        {"SB", "bne s1, s8, target",
         {.address = 0x100c, .line = 4, .mnemonic = "bne", .operand_count = 3,
          .operands = {REG(9), REG(24), LABEL("target")}}},
        {"U", "lui s3, 0x53a4b",
         {.address = 0x1010, .line = 5, .mnemonic = "lui", .operand_count = 2,
          .operands = {REG(19), IMM(0x53a4b)}}},
        {"UJ", "jal ra, target",
         {.address = 0x1014, .line = 6, .mnemonic = "jal", .operand_count = 2,
          .operands = {REG(1), LABEL("target")}}},
        {"I", "target: ld t0, 40(a0)",
         {.address = 0x1018, .line = 7, .mnemonic = "ld", .operand_count = 2,
          .operands = {REG(5), MEM(10, 40)}}},
        {"S", "sd t0, 40(a0)",
         {.address = 0x101c, .line = 8, .mnemonic = "sd", .operand_count = 2,
          .operands = {REG(5), MEM(10, 40)}}}
    };
    const RvSymbol symbols[] = {{"target", 0x1018}};
    const size_t count = sizeof examples / sizeof examples[0];
    uint32_t words[sizeof examples / sizeof examples[0]];
    RvError error;

    for (size_t i = 0; i < count; ++i) {
        if (!rv64i_encode(&examples[i].instruction, symbols, 1, &words[i], &error)) {
            fprintf(stderr, "line %zu: %s\n", error.line, error.message);
            return EXIT_FAILURE;
        }
    }
    puts("Address           Format  Machine code (binary)             Hex       Assembly");
    for (size_t i = 0; i < count; ++i) {
        char bits[33];
        for (unsigned bit = 0; bit < 32; ++bit)
            bits[bit] = (char)('0' + ((words[i] >> (31 - bit)) & 1));
        bits[32] = '\0';
        printf("%016" PRIx64 "  %-6s  %s  %08" PRIx32 "  %s\n",
               examples[i].instruction.address, examples[i].format,
               bits, words[i], examples[i].assembly);
    }
    return EXIT_SUCCESS;
}
