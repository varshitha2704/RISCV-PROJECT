#include "rv64i.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define REG(n) { .kind = RV_REGISTER, .value.reg = (n) }
#define IMM(n) { .kind = RV_IMMEDIATE, .value.immediate = (n) }
#define MEM(r, n) { .kind = RV_MEMORY, .value.memory = { (r), (n) } }
#define LABEL(s) { .kind = RV_LABEL, .value.label = (s) }
#define LENGTH(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    const char *mnemonic;
    size_t operand_count;
    RvOperand operands[3];
    uint32_t expected_word;
} EncodingTest;

/* The 96 reference words were checked with LLVM 22.1.0's assembler. */
static const EncodingTest instruction_cases[] = {
    { "add",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003100b3) },
    { "sub",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x403100b3) },
    { "sll",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003110b3) },
    { "slt",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003120b3) },
    { "sltu",  3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003130b3) },
    { "xor",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003140b3) },
    { "srl",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003150b3) },
    { "sra",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x403150b3) },
    { "or",    3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003160b3) },
    { "and",   3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003170b3) },
    { "addi",  3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff10093) },
    { "slti",  3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff12093) },
    { "sltiu", 3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff13093) },
    { "xori",  3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff14093) },
    { "ori",   3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff16093) },
    { "andi",  3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff17093) },
    { "slli",  3, { REG(1), REG(2), IMM(63) }, UINT32_C(0x03f11093) },
    { "srli",  3, { REG(1), REG(2), IMM(63) }, UINT32_C(0x03f15093) },
    { "srai",  3, { REG(1), REG(2), IMM(63) }, UINT32_C(0x43f15093) },
    { "lb",    2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c10083) },
    { "lh",    2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c11083) },
    { "lw",    2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c12083) },
    { "lbu",   2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c14083) },
    { "lhu",   2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c15083) },
    { "sb",    2, { REG(3), MEM(2, 8) }, UINT32_C(0x00310423) },
    { "sh",    2, { REG(3), MEM(2, 8) }, UINT32_C(0x00311423) },
    { "sw",    2, { REG(3), MEM(2, 8) }, UINT32_C(0x00312423) },
    { "beq",   3, { REG(1), REG(2), IMM(8) }, UINT32_C(0x00208463) },
    { "bne",   3, { REG(1), REG(2), IMM(8) }, UINT32_C(0x00209463) },
    { "blt",   3, { REG(1), REG(2), IMM(8) }, UINT32_C(0x0020c463) },
    { "bge",   3, { REG(1), REG(2), IMM(8) }, UINT32_C(0x0020d463) },
    { "bltu",  3, { REG(1), REG(2), IMM(8) }, UINT32_C(0x0020e463) },
    { "bgeu",  3, { REG(1), REG(2), IMM(8) }, UINT32_C(0x0020f463) },
    { "jal",   2, { REG(1), IMM(8) }, UINT32_C(0x008000ef) },
    { "jalr",  2, { REG(1), MEM(2, -1) }, UINT32_C(0xfff100e7) },
    { "lui",   2, { REG(1), IMM(0x12345) }, UINT32_C(0x123450b7) },
    { "auipc", 2, { REG(1), IMM(0x12345) }, UINT32_C(0x12345097) },
    { "fence", 2, { IMM(15), IMM(15) }, UINT32_C(0x0ff0000f) },
    { "ecall", 0, { { 0 } }, UINT32_C(0x00000073) },
    { "ebreak",0, { { 0 } }, UINT32_C(0x00100073) },
    { "ld",    2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c13083) },
    { "lwu",   2, { REG(1), MEM(2, 12) }, UINT32_C(0x00c16083) },
    { "sd",    2, { REG(3), MEM(2, 8) }, UINT32_C(0x00313423) },
    { "addiw", 3, { REG(1), REG(2), IMM(-1) }, UINT32_C(0xfff1009b) },
    { "slliw", 3, { REG(1), REG(2), IMM(31) }, UINT32_C(0x01f1109b) },
    { "srliw", 3, { REG(1), REG(2), IMM(31) }, UINT32_C(0x01f1509b) },
    { "sraiw", 3, { REG(1), REG(2), IMM(31) }, UINT32_C(0x41f1509b) },
    { "addw",  3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003100bb) },
    { "subw",  3, { REG(1), REG(2), REG(3) }, UINT32_C(0x403100bb) },
    { "sllw",  3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003110bb) },
    { "srlw",  3, { REG(1), REG(2), REG(3) }, UINT32_C(0x003150bb) },
    { "sraw",  3, { REG(1), REG(2), REG(3) }, UINT32_C(0x403150bb) }
};

_Static_assert(LENGTH(instruction_cases) == 52, "Expected one test for each of the 52 instructions");

static size_t tests_run;
static size_t tests_failed;

static void expect_word(const char *name, const RvInstruction *instruction,
                        const RvSymbol *symbols, size_t symbol_count, uint32_t expected)
{
    uint32_t word = UINT32_C(0xa5a5a5a5);
    RvError error = { 0 };
    bool ok = rv64i_encode(instruction, symbols, symbol_count, &word, &error);
    ++tests_run;
    if (!ok || word != expected) {
        ++tests_failed;
        fprintf(stderr, "FAIL %s: expected %08" PRIx32 ", got %08" PRIx32
                " (ok=%d, error=%s)\n", name, expected, word, ok, error.message);
    }
}

static void expect_error(const char *name, const RvInstruction *instruction,
                         const RvSymbol *symbols, size_t symbol_count)
{
    uint32_t word = UINT32_C(0xa5a5a5a5);
    RvError error = { 0 };
    bool ok = rv64i_encode(instruction, symbols, symbol_count, &word, &error);
    ++tests_run;
    if (ok || word != UINT32_C(0xa5a5a5a5) || error.message[0] == '\0' ||
        (instruction != NULL && error.line != instruction->line)) {
        ++tests_failed;
        fprintf(stderr, "FAIL %s: expected error with unchanged output and source line\n", name);
    }
}

static void test_mnemonics(void)
{
    for (size_t n = 0; n < LENGTH(instruction_cases); ++n) {
        RvInstruction instruction = {
            .address = UINT64_C(0x1000),
            .line = n + 101,
            .mnemonic = instruction_cases[n].mnemonic,
            .operand_count = instruction_cases[n].operand_count
        };
        memcpy(instruction.operands, instruction_cases[n].operands, sizeof(instruction.operands));
        expect_word(instruction.mnemonic, &instruction, NULL, 0, instruction_cases[n].expected_word);

        RvInstruction invalid = instruction;
        if (instruction.operand_count == 0)
            invalid.operand_count = 1;
        else
            invalid.operand_count = instruction.operand_count - 1;
        expect_error("wrong operand count", &invalid, NULL, 0);
        invalid.operand_count = 4;
        expect_error("count exceeds operand array", &invalid, NULL, 0);
        for (size_t operand = 0; operand < instruction.operand_count; ++operand) {
            invalid = instruction;
            if (instruction.operands[operand].kind == RV_REGISTER)
                invalid.operands[operand] = (RvOperand)IMM(0);
            else
                invalid.operands[operand] = (RvOperand)REG(0);
            expect_error("wrong operand type", &invalid, NULL, 0);
            invalid = instruction;
            if (instruction.operands[operand].kind == RV_REGISTER) {
                invalid.operands[operand].value.reg = 32;
                expect_error("register x32", &invalid, NULL, 0);
            } else if (instruction.operands[operand].kind == RV_MEMORY) {
                invalid.operands[operand].value.memory.base = 32;
                expect_error("memory base x32", &invalid, NULL, 0);
            }
        }
    }
}

static void test_boundaries(void)
{
    static const EncodingTest valid[] = {
        { "add", 3, { REG(0), REG(0), REG(0) }, UINT32_C(0x00000033) },
        { "add", 3, { REG(31), REG(31), REG(31) }, UINT32_C(0x01ff8fb3) },
        { "addi", 3, { REG(31), REG(0), IMM(-2048) }, UINT32_C(0x80000f93) },
        { "addi", 3, { REG(31), REG(0), IMM(2047) }, UINT32_C(0x7ff00f93) },
        { "slli", 3, { REG(1), REG(2), IMM(0) }, UINT32_C(0x00011093) },
        { "slliw", 3, { REG(1), REG(2), IMM(0) }, UINT32_C(0x0001109b) },
        { "lw", 2, { REG(1), MEM(2, -2048) }, UINT32_C(0x80012083) },
        { "lw", 2, { REG(1), MEM(2, 2047) }, UINT32_C(0x7ff12083) },
        { "lw", 2, { REG(5), MEM(8, 100) }, UINT32_C(0x06442283) },
        { "sw", 2, { REG(3), MEM(2, -2048) }, UINT32_C(0x80312023) },
        { "sw", 2, { REG(3), MEM(2, 2047) }, UINT32_C(0x7e312fa3) },
        { "jalr", 2, { REG(1), MEM(2, -2048) }, UINT32_C(0x800100e7) },
        { "jalr", 2, { REG(1), MEM(2, 2047) }, UINT32_C(0x7ff100e7) },
        { "lui", 2, { REG(1), IMM(0) }, UINT32_C(0x000000b7) },
        { "lui", 2, { REG(1), IMM(0xfffff) }, UINT32_C(0xfffff0b7) },
        { "beq", 3, { REG(0), REG(0), IMM(-4096) }, UINT32_C(0x80000063) },
        { "beq", 3, { REG(0), REG(0), IMM(4094) }, UINT32_C(0x7e000fe3) },
        { "beq", 3, { REG(0), REG(0), IMM(2048) }, UINT32_C(0x000000e3) },
        { "beq", 3, { REG(0), REG(0), IMM(2) }, UINT32_C(0x00000163) },
        { "beq", 3, { REG(0), REG(0), IMM(-2) }, UINT32_C(0xfe000fe3) },
        { "jal", 2, { REG(1), IMM(-1048576) }, UINT32_C(0x800000ef) },
        { "jal", 2, { REG(1), IMM(1048574) }, UINT32_C(0x7ffff0ef) },
        { "jal", 2, { REG(1), IMM(2048) }, UINT32_C(0x001000ef) },
        { "jal", 2, { REG(1), IMM(524288) }, UINT32_C(0x000800ef) },
        { "jal", 2, { REG(1), IMM(2) }, UINT32_C(0x002000ef) },
        { "jal", 2, { REG(1), IMM(-2) }, UINT32_C(0xfffff0ef) },
        { "fence", 2, { IMM(0), IMM(0) }, UINT32_C(0x0000000f) },
        { "fence", 2, { IMM(8), IMM(1) }, UINT32_C(0x0810000f) },
        { "fence", 2, { IMM(2), IMM(4) }, UINT32_C(0x0240000f) }
    };
    for (size_t n = 0; n < LENGTH(valid); ++n) {
        RvInstruction instruction = {
            .address = UINT64_C(0x1000),
            .line = n + 201,
            .mnemonic = valid[n].mnemonic,
            .operand_count = valid[n].operand_count
        };
        memcpy(instruction.operands, valid[n].operands, sizeof(instruction.operands));
        expect_word("valid boundary", &instruction, NULL, 0, valid[n].expected_word);
    }

    static const struct { const char *mnemonic; int64_t low; int64_t high; } limits[] = {
        { "addi", -2048, 2047 }, { "slti", -2048, 2047 }, { "sltiu", -2048, 2047 },
        { "xori", -2048, 2047 }, { "ori", -2048, 2047 }, { "andi", -2048, 2047 },
        { "addiw", -2048, 2047 }, { "slli", 0, 63 }, { "srli", 0, 63 },
        { "srai", 0, 63 }, { "slliw", 0, 31 }, { "srliw", 0, 31 }, { "sraiw", 0, 31 }
    };
    for (size_t n = 0; n < LENGTH(limits); ++n) {
        const int64_t bad[] = { limits[n].low - 1, limits[n].high + 1, INT64_MIN, INT64_MAX };
        RvInstruction instruction = {
            .address = 0, .line = 301, .mnemonic = limits[n].mnemonic,
            .operand_count = 3, .operands = { REG(1), REG(2), IMM(0) }
        };
        for (size_t i = 0; i < LENGTH(bad); ++i) {
            instruction.operands[2].value.immediate = bad[i];
            expect_error("immediate outside range", &instruction, NULL, 0);
        }
    }
    static const char *const memory[] = { "lb", "lh", "lw", "lbu", "lhu", "ld", "lwu", "sb", "sh", "sw", "sd", "jalr" };
    for (size_t n = 0; n < LENGTH(memory); ++n) {
        const int64_t bad[] = { -2049, 2048, INT64_MIN, INT64_MAX };
        RvInstruction instruction = {
            .address = 0, .line = 302, .mnemonic = memory[n],
            .operand_count = 2, .operands = { REG(1), MEM(2, 0) }
        };
        for (size_t i = 0; i < LENGTH(bad); ++i) {
            instruction.operands[1].value.memory.offset = bad[i];
            expect_error("memory offset outside range", &instruction, NULL, 0);
        }
    }
    static const char *const upper[] = { "lui", "auipc" };
    for (size_t n = 0; n < LENGTH(upper); ++n) {
        const int64_t bad[] = { -1, 0x100000, INT64_MIN, INT64_MAX };
        RvInstruction instruction = {
            .address = 0, .line = 303, .mnemonic = upper[n],
            .operand_count = 2, .operands = { REG(1), IMM(0) }
        };
        for (size_t i = 0; i < LENGTH(bad); ++i) {
            instruction.operands[1].value.immediate = bad[i];
            expect_error("upper immediate outside range", &instruction, NULL, 0);
        }
    }
    RvInstruction fence = {
        .address = 0, .line = 304, .mnemonic = "fence",
        .operand_count = 2, .operands = { IMM(0), IMM(0) }
    };
    for (size_t operand = 0; operand < 2; ++operand) {
        const int64_t bad[] = { -1, 16, INT64_MIN, INT64_MAX };
        for (size_t i = 0; i < LENGTH(bad); ++i) {
            fence.operands[operand].value.immediate = bad[i];
            expect_error("fence mask outside range", &fence, NULL, 0);
        }
        fence.operands[operand].value.immediate = 0;
    }
}

static void test_targets(void)
{
    static const char *const branches[] = { "beq", "bne", "blt", "bge", "bltu", "bgeu", "jal" };
    for (size_t n = 0; n < LENGTH(branches); ++n) {
        RvInstruction instruction = {
            .address = 0, .line = 401, .mnemonic = branches[n],
            .operand_count = 3, .operands = { REG(1), REG(2), IMM(0) }
        };
        size_t target_operand = 2;
        int64_t minimum_offset = -4096;
        int64_t maximum_offset = 4094;
        if (strcmp(branches[n], "jal") == 0) {
            instruction.operand_count = 2;
            target_operand = 1;
            minimum_offset = -1048576;
            maximum_offset = 1048574;
        }
        const int64_t bad[] = {
            -1, 1, minimum_offset - 2, maximum_offset + 2, INT64_MIN, INT64_MAX
        };
        instruction.operands[target_operand] = (RvOperand)IMM(0);
        for (size_t i = 0; i < LENGTH(bad); ++i) {
            instruction.operands[target_operand].value.immediate = bad[i];
            expect_error("branch or jump displacement invalid", &instruction, NULL, 0);
        }
    }

    RvInstruction branch = {
        .address = UINT64_C(0x1000), .line = 402, .mnemonic = "beq",
        .operand_count = 3, .operands = { REG(1), REG(2), LABEL("target") }
    };
    RvInstruction jump = {
        .address = UINT64_C(0x1000), .line = 403, .mnemonic = "jal",
        .operand_count = 2, .operands = { REG(1), LABEL("target") }
    };
    RvSymbol symbol = { "target", UINT64_C(0x1008) };
    expect_word("forward branch label", &branch, &symbol, 1, UINT32_C(0x00208463));
    expect_word("forward jump label", &jump, &symbol, 1, UINT32_C(0x008000ef));
    symbol.address = UINT64_C(0xff8);
    expect_word("backward branch label", &branch, &symbol, 1, UINT32_C(0xfe208ce3));
    expect_word("backward jump label", &jump, &symbol, 1, UINT32_C(0xff9ff0ef));
    symbol.address = UINT64_C(0x1000);
    expect_word("branch to self", &branch, &symbol, 1, UINT32_C(0x00208063));
    expect_word("jump to self", &jump, &symbol, 1, UINT32_C(0x000000ef));
    symbol.address = UINT64_C(0x1002);
    expect_word("two-byte branch label displacement", &branch, &symbol, 1, UINT32_C(0x00208163));
    expect_word("two-byte jump label displacement", &jump, &symbol, 1, UINT32_C(0x002000ef));
    symbol.address = 0;
    expect_word("minimum branch label displacement", &branch, &symbol, 1, UINT32_C(0x80208063));
    symbol.address = UINT64_C(0x1ffe);
    expect_word("maximum branch label displacement", &branch, &symbol, 1, UINT32_C(0x7e208fe3));
    symbol.address = UINT64_C(0x2000);
    expect_error("branch label one step beyond range", &branch, &symbol, 1);
    symbol.address = UINT64_C(0x100ffe);
    expect_word("maximum jump label displacement", &jump, &symbol, 1, UINT32_C(0x7ffff0ef));
    symbol.address = UINT64_C(0x101000);
    expect_error("jump label one step beyond range", &jump, &symbol, 1);
    jump.address = UINT64_C(0x100000);
    symbol.address = 0;
    expect_word("minimum jump label displacement", &jump, &symbol, 1, UINT32_C(0x800000ef));

    branch.address = UINT64_MAX - 3;
    jump.address = UINT64_MAX - 3;
    symbol.address = 4;
    expect_word("forward branch across address wrap", &branch, &symbol, 1, UINT32_C(0x00208463));
    expect_word("forward jump across address wrap", &jump, &symbol, 1, UINT32_C(0x008000ef));
    branch.address = 4;
    jump.address = 4;
    symbol.address = UINT64_MAX - 3;
    expect_word("backward branch across address wrap", &branch, &symbol, 1, UINT32_C(0xfe208ce3));
    expect_word("backward jump across address wrap", &jump, &symbol, 1, UINT32_C(0xff9ff0ef));

    const uint64_t far[] = { UINT64_C(0x8000000000000004), UINT64_C(0x100000004), UINT64_C(0x200004) };
    for (size_t n = 0; n < LENGTH(far); ++n) {
        symbol.address = far[n];
        expect_error("far branch label must not truncate", &branch, &symbol, 1);
        expect_error("far jump label must not truncate", &jump, &symbol, 1);
    }
    symbol.address = 5;
    expect_error("odd branch label", &branch, &symbol, 1);
    expect_error("odd jump label", &jump, &symbol, 1);
    expect_error("missing branch symbol", &branch, NULL, 0);
    expect_error("missing jump symbol", &jump, NULL, 0);
    symbol.name = "other";
    expect_error("unknown label", &branch, &symbol, 1);
    symbol.name = NULL;
    expect_error("null symbol name", &branch, &symbol, 1);
    symbol.name = "";
    expect_error("empty symbol name", &branch, &symbol, 1);
    const RvSymbol duplicate[] = { { "target", 4 }, { "target", 8 } };
    expect_error("duplicate branch symbol", &branch, duplicate, LENGTH(duplicate));
    expect_error("duplicate jump symbol", &jump, duplicate, LENGTH(duplicate));
    const RvSymbol identical[] = { { "target", 4 }, { "target", 4 } };
    expect_error("duplicate symbol even at same address", &branch, identical, LENGTH(identical));
    branch.operands[2].value.label = NULL;
    expect_error("null label name", &branch, NULL, 0);
    jump.operands[1].value.label = "";
    expect_error("empty label name", &jump, NULL, 0);
}

static void test_api_errors(void)
{
    RvInstruction instruction = {
        .address = 0, .line = 501, .mnemonic = "add",
        .operand_count = 3, .operands = { REG(1), REG(2), REG(3) }
    };
    expect_error("null instruction", NULL, NULL, 0);
    expect_error("null symbol table with nonzero count", &instruction, NULL, 1);
    instruction.address = 2;
    expect_error("unaligned instruction address", &instruction, NULL, 0);
    instruction.address = 0;
    instruction.mnemonic = NULL;
    expect_error("null mnemonic", &instruction, NULL, 0);
    static const char *const unsupported[] = {
        "", "ADD", "unknown", "nop", "mv", "li", "ret", "j", "call",
        "mul", "div", "csrrw", "fence.i", "flw", "c.addi"
    };
    for (size_t n = 0; n < LENGTH(unsupported); ++n) {
        instruction.mnemonic = unsupported[n];
        expect_error("unsupported instruction", &instruction, NULL, 0);
    }
    instruction.mnemonic = "add";
    instruction.operands[0].kind = (RvOperandKind)99;
    expect_error("invalid operand kind", &instruction, NULL, 0);
    instruction.operands[0] = (RvOperand)REG(1);
    instruction.operands[0].value.reg = UINT_MAX;
    expect_error("oversized unsigned register", &instruction, NULL, 0);
    instruction.operands[0] = (RvOperand)REG(1);
    instruction.operand_count = SIZE_MAX;
    expect_error("huge operand count", &instruction, NULL, 0);
    instruction.operand_count = 3;

    RvError error = { 0 };
    ++tests_run;
    if (rv64i_encode(&instruction, NULL, 0, NULL, &error) ||
        error.line != instruction.line || error.message[0] == '\0') {
        ++tests_failed;
        fprintf(stderr, "FAIL null output pointer\n");
    }
    uint32_t word = 0;
    ++tests_run;
    if (!rv64i_encode(&instruction, NULL, 0, &word, NULL) || word != UINT32_C(0x003100b3)) {
        ++tests_failed;
        fprintf(stderr, "FAIL optional error pointer on success\n");
    }
    instruction.operands[0].value.reg = 32;
    ++tests_run;
    if (rv64i_encode(&instruction, NULL, 0, &word, NULL) || word != UINT32_C(0x003100b3)) {
        ++tests_failed;
        fprintf(stderr, "FAIL optional error pointer on failure\n");
    }
}

static void test_class_examples(void)
{
    /* Examples from Topic2 classnotes. Part 1 converts the register aliases. */
    static const EncodingTest class_examples[] = {
        /* p37: add s1, s4, s5 */
        { "add", 3, { REG(9), REG(20), REG(21) }, UINT32_C(0x015a04b3) },
        /* p38: sub t1, a3, s4 */
        { "sub", 3, { REG(6), REG(13), REG(20) }, UINT32_C(0x41468333) },
        /* p39: addi t5, s7, 100 */
        { "addi", 3, { REG(30), REG(23), IMM(100) }, UINT32_C(0x064b8f13) },
        /* p40: addi t0, zero, -101 */
        { "addi", 3, { REG(5), REG(0), IMM(-101) }, UINT32_C(0xf9b00293) },
        /* p41: ori a0, s1, 0x7c; and s0, s1, s2 */
        { "ori", 3, { REG(10), REG(9), IMM(0x7c) }, UINT32_C(0x07c4e513) },
        { "and", 3, { REG(8), REG(9), REG(18) }, UINT32_C(0x0124f433) },
        /* p42: slli a1, s3, 4 */
        { "slli", 3, { REG(11), REG(19), IMM(4) }, UINT32_C(0x00499593) },
        /* p43: srai s0, a0, 4 */
        { "srai", 3, { REG(8), REG(10), IMM(4) }, UINT32_C(0x40455413) },
        /* p44: lui s3, 0x53a4b */
        { "lui", 2, { REG(19), IMM(0x53a4b) }, UINT32_C(0x53a4b9b7) },
        /* p45: bne s1, s8, exit; displacement +12 */
        { "bne", 3, { REG(9), REG(24), IMM(12) }, UINT32_C(0x01849663) },
        /* p46: beq zero, zero, loop; displacement -20 */
        { "beq", 3, { REG(0), REG(0), IMM(-20) }, UINT32_C(0xfe0006e3) },
        /* p47: use 0x800a0 as the byte offset; the source PC isn't given. */
        { "jal", 2, { REG(1), IMM(0x800a0) }, UINT32_C(0x0a0800ef) },
        /* p49: sw t0, 40(a0) */
        { "sw", 2, { REG(5), MEM(10, 40) }, UINT32_C(0x02552423) },
        /* p50: sub a1, a0, s1 */
        { "sub", 3, { REG(11), REG(10), REG(9) }, UINT32_C(0x409505b3) },
        /* p26: jalr zero, 0(ra) */
        { "jalr", 2, { REG(0), MEM(1, 0) }, UINT32_C(0x00008067) }
    };
    for (size_t n = 0; n < LENGTH(class_examples); ++n) {
        RvInstruction instruction = {
            .address = UINT64_C(0x1000),
            .line = n + 1,
            .mnemonic = class_examples[n].mnemonic,
            .operand_count = class_examples[n].operand_count
        };
        memcpy(instruction.operands, class_examples[n].operands,
               sizeof instruction.operands);
        expect_word("lecture encoding example", &instruction, NULL, 0,
                    class_examples[n].expected_word);
    }

    /* Use the PCs from page 46 to test label lookup and branch offsets. */
    const RvSymbol symbols[] = { { "loop", 0xa0 }, { "exit", 0xb8 },
                                 { "jump_target", 0x810a0 } };
    const RvInstruction forward = {
        .address = 0xac, .line = 4, .mnemonic = "bne",
        .operand_count = 3, .operands = { REG(9), REG(24), LABEL("exit") }
    };
    const RvInstruction backward = {
        .address = 0xb4, .line = 6, .mnemonic = "beq",
        .operand_count = 3, .operands = { REG(0), REG(0), LABEL("loop") }
    };
    const RvInstruction jump = {
        .address = 0x1000, .line = 1, .mnemonic = "jal",
        .operand_count = 2, .operands = { REG(1), LABEL("jump_target") }
    };
    expect_word("lecture forward branch PC", &forward, symbols, LENGTH(symbols),
                UINT32_C(0x01849663));
    expect_word("lecture backward branch PC", &backward, symbols, LENGTH(symbols),
                UINT32_C(0xfe0006e3));
    expect_word("adapted lecture jump with explicit PC", &jump, symbols, LENGTH(symbols),
                UINT32_C(0x0a0800ef));
}

int main(void)
{
    test_mnemonics();
    test_boundaries();
    test_targets();
    test_api_errors();
    test_class_examples();
    printf("%zu/%zu tests passed (%zu instructions).\n",
           tests_run - tests_failed, tests_run, LENGTH(instruction_cases));
    return tests_failed == 0 ? 0 : 1;
}
