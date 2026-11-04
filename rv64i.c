#include "rv64i.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    R, I, SHIFT64, SHIFT32, LOAD, STORE, BRANCH, UPPER, JUMP, JALR,
    FENCE, SYSTEM
} Form;

/* The class notes call BRANCH and JUMP formats SB and UJ. */

typedef struct {
    const char *name;
    Form form;
    uint32_t opcode;
    uint32_t funct3;
    uint32_t extra_bits;
} Encoding;

/* Bit 30 selects subtraction/arithmetic shifts; bit 20 selects EBREAK. */
static const Encoding encodings[] = {
    /* name     form      opcode  funct3  extra_bits */
    {"add",    R,        0x33,    0,      0},
    {"sub",    R,        0x33,    0,      UINT32_C(1) << 30},
    {"sll",    R,        0x33,    1,      0},
    {"slt",    R,        0x33,    2,      0},
    {"sltu",   R,        0x33,    3,      0},
    {"xor",    R,        0x33,    4,      0},
    {"srl",    R,        0x33,    5,      0},
    {"sra",    R,        0x33,    5,      UINT32_C(1) << 30},
    {"or",     R,        0x33,    6,      0},
    {"and",    R,        0x33,    7,      0},
    {"addw",   R,        0x3b,    0,      0},
    {"subw",   R,        0x3b,    0,      UINT32_C(1) << 30},
    {"sllw",   R,        0x3b,    1,      0},
    {"srlw",   R,        0x3b,    5,      0},
    {"sraw",   R,        0x3b,    5,      UINT32_C(1) << 30},
    {"addi",   I,        0x13,    0,      0},
    {"slti",   I,        0x13,    2,      0},
    {"sltiu",  I,        0x13,    3,      0},
    {"xori",   I,        0x13,    4,      0},
    {"ori",    I,        0x13,    6,      0},
    {"andi",   I,        0x13,    7,      0},
    {"addiw",  I,        0x1b,    0,      0},
    {"slli",   SHIFT64,  0x13,    1,      0},
    {"srli",   SHIFT64,  0x13,    5,      0},
    {"srai",   SHIFT64,  0x13,    5,      UINT32_C(1) << 30},
    {"slliw",  SHIFT32,  0x1b,    1,      0},
    {"srliw",  SHIFT32,  0x1b,    5,      0},
    {"sraiw",  SHIFT32,  0x1b,    5,      UINT32_C(1) << 30},
    {"lb",     LOAD,     0x03,    0,      0},
    {"lh",     LOAD,     0x03,    1,      0},
    {"lw",     LOAD,     0x03,    2,      0},
    {"ld",     LOAD,     0x03,    3,      0},
    {"lbu",    LOAD,     0x03,    4,      0},
    {"lhu",    LOAD,     0x03,    5,      0},
    {"lwu",    LOAD,     0x03,    6,      0},
    {"sb",     STORE,    0x23,    0,      0},
    {"sh",     STORE,    0x23,    1,      0},
    {"sw",     STORE,    0x23,    2,      0},
    {"sd",     STORE,    0x23,    3,      0},
    {"beq",    BRANCH,   0x63,    0,      0},
    {"bne",    BRANCH,   0x63,    1,      0},
    {"blt",    BRANCH,   0x63,    4,      0},
    {"bge",    BRANCH,   0x63,    5,      0},
    {"bltu",   BRANCH,   0x63,    6,      0},
    {"bgeu",   BRANCH,   0x63,    7,      0},
    {"lui",    UPPER,    0x37,    0,      0},
    {"auipc",  UPPER,    0x17,    0,      0},
    {"jal",    JUMP,     0x6f,    0,      0},
    {"jalr",   JALR,     0x67,    0,      0},
    {"fence",  FENCE,    0x0f,    0,      0},
    {"ecall",  SYSTEM,   0x73,    0,      0},
    {"ebreak", SYSTEM,   0x73,    0,      UINT32_C(1) << 20},
};

static bool fail(RvError *error, const char *format, ...)
{
    if (error != NULL) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
    }
    return false;
}

static bool get_register(const RvOperand *op, size_t position,
                         uint32_t *reg, RvError *error)
{
    if (op->kind != RV_REGISTER)
        return fail(error, "operand %zu must be a register", position);
    if (op->value.reg > 31)
        return fail(error, "operand %zu: register must be x0 through x31", position);
    *reg = op->value.reg;
    return true;
}

static bool get_immediate(const RvOperand *op, size_t position,
                          int64_t low, int64_t high,
                          int64_t *value, RvError *error)
{
    if (op->kind != RV_IMMEDIATE)
        return fail(error, "operand %zu must be an integer immediate", position);
    if (op->value.immediate < low || op->value.immediate > high)
        return fail(error, "operand %zu: immediate must be between %" PRId64
                    " and %" PRId64, position, low, high);
    *value = op->value.immediate;
    return true;
}

static bool get_memory(const RvOperand *op, uint32_t *base,
                       int64_t *offset, RvError *error)
{
    if (op->kind != RV_MEMORY)
        return fail(error, "operand 2 must be a memory operand: offset(base)");
    if (op->value.memory.base > 31)
        return fail(error, "operand 2: base register must be x0 through x31");
    if (op->value.memory.offset < -2048 || op->value.memory.offset > 2047)
        return fail(error, "operand 2: memory offset must be between -2048 and 2047");
    *base = op->value.memory.base;
    *offset = op->value.memory.offset;
    return true;
}

static bool get_displacement(const RvOperand *op, size_t position,
                            uint64_t address, const RvSymbol *symbols,
                            size_t symbol_count, int64_t low, int64_t high,
                            int64_t *value, RvError *error)
{
    if (op->kind == RV_IMMEDIATE) {
        if (!get_immediate(op, position, low, high, value, error))
            return false;
    } else if (op->kind == RV_LABEL) {
        if (op->value.label == NULL || op->value.label[0] == '\0')
            return fail(error, "operand %zu: label must have a name", position);
        bool found = false;
        uint64_t target = 0;
        for (size_t i = 0; i < symbol_count; ++i) {
            if (symbols[i].name == NULL || symbols[i].name[0] == '\0')
                return fail(error, "symbol table contains an unnamed label");
            if (strcmp(op->value.label, symbols[i].name) == 0) {
                if (found)
                    return fail(error, "duplicate label: %s", op->value.label);
                target = symbols[i].address;
                found = true;
            }
        }
        if (!found)
            return fail(error, "undefined label: %s", op->value.label);

        /* Unsigned subtraction keeps RV64 address wraparound defined. */
        uint64_t forward_distance = target - address;
        uint64_t backward_distance = address - target;
        if (forward_distance <= (uint64_t)high) {
            *value = (int64_t)forward_distance;
        } else if (backward_distance <= (uint64_t)(-low)) {
            *value = -(int64_t)backward_distance;
        } else {
            return fail(error, "label out of range: %s", op->value.label);
        }
    } else {
        return fail(error, "operand %zu must be a byte displacement or label", position);
    }
    if (*value % 2 != 0)
        return fail(error, "operand %zu: branch/jump displacement must be even", position);
    return true;
}

bool rv64i_encode(const RvInstruction *instruction,
                  const RvSymbol *symbols, size_t symbol_count,
                  uint32_t *word, RvError *error)
{
    if (error != NULL) {
        error->line = instruction == NULL ? 0 : instruction->line;
        error->message[0] = '\0';
    }
    if (instruction == NULL || word == NULL)
        return fail(error, "instruction and output word are required");
    if (symbols == NULL && symbol_count != 0)
        return fail(error, "symbol table is required when symbol_count is nonzero");
    if (instruction->mnemonic == NULL || instruction->mnemonic[0] == '\0')
        return fail(error, "instruction mnemonic is required");
    if (instruction->address % 4 != 0)
        return fail(error, "instruction address must be a multiple of 4");

    const Encoding *encoding = NULL;
    for (size_t i = 0; i < sizeof encodings / sizeof encodings[0]; ++i) {
        if (strcmp(instruction->mnemonic, encodings[i].name) == 0) {
            encoding = &encodings[i];
            break;
        }
    }
    if (encoding == NULL)
        return fail(error, "unsupported instruction: %s", instruction->mnemonic);

    size_t count = 2;
    switch (encoding->form) {
    case R: case I: case SHIFT64: case SHIFT32: case BRANCH:
        count = 3;
        break;
    case SYSTEM:
        count = 0;
        break;
    default:
        break;
    }
    if (instruction->operand_count != count)
        return fail(error, "%s expects %zu operands, got %zu",
                    instruction->mnemonic, count, instruction->operand_count);

    const RvOperand *op = instruction->operands;
    uint32_t rd = 0, rs1 = 0, rs2 = 0, imm_bits = 0;
    int64_t immediate = 0;
    uint32_t result = encoding->opcode;
    result |= encoding->funct3 << 12;
    result |= encoding->extra_bits;

    switch (encoding->form) {
    case R:
        if (!get_register(&op[0], 1, &rd, error) ||
            !get_register(&op[1], 2, &rs1, error) ||
            !get_register(&op[2], 3, &rs2, error))
            return false;
        result |= rd << 7;
        result |= rs1 << 15;
        result |= rs2 << 20;
        break;
    case I: case SHIFT64: case SHIFT32: {
        int64_t minimum = -2048;
        int64_t maximum = 2047;
        if (encoding->form == SHIFT64) {
            minimum = 0;
            maximum = 63;
        } else if (encoding->form == SHIFT32) {
            minimum = 0;
            maximum = 31;
        }
        if (!get_register(&op[0], 1, &rd, error) ||
            !get_register(&op[1], 2, &rs1, error))
            return false;
        if (!get_immediate(&op[2], 3, minimum, maximum,
                           &immediate, error))
            return false;
        imm_bits = (uint32_t)immediate & 0xfff;
        result |= rd << 7;
        result |= rs1 << 15;
        result |= imm_bits << 20;
        break;
    }
    case LOAD: case JALR:
        if (!get_register(&op[0], 1, &rd, error) ||
            !get_memory(&op[1], &rs1, &immediate, error))
            return false;
        imm_bits = (uint32_t)immediate & 0xfff;
        result |= rd << 7;
        result |= rs1 << 15;
        result |= imm_bits << 20;
        break;
    case STORE:
        if (!get_register(&op[0], 1, &rs2, error) ||
            !get_memory(&op[1], &rs1, &immediate, error))
            return false;
        imm_bits = (uint32_t)immediate;
        /* S format splits the immediate around the register fields. */
        result |= rs1 << 15;
        result |= rs2 << 20;
        result |= ((imm_bits >> 5) & 0x7f) << 25;
        result |= (imm_bits & 0x1f) << 7;
        break;
    case BRANCH:
        if (!get_register(&op[0], 1, &rs1, error) ||
            !get_register(&op[1], 2, &rs2, error) ||
            !get_displacement(&op[2], 3, instruction->address, symbols,
                              symbol_count, -4096, 4094, &immediate, error))
            return false;
        imm_bits = (uint32_t)immediate;
        /* SB/B stores imm[12|10:5|4:1|11]; bit 0 is always zero. */
        result |= rs1 << 15;
        result |= rs2 << 20;
        result |= ((imm_bits >> 12) & 0x1) << 31;
        result |= ((imm_bits >> 5) & 0x3f) << 25;
        result |= ((imm_bits >> 1) & 0xf) << 8;
        result |= ((imm_bits >> 11) & 0x1) << 7;
        break;
    case UPPER:
        if (!get_register(&op[0], 1, &rd, error) ||
            !get_immediate(&op[1], 2, 0, 0xfffff, &immediate, error))
            return false;
        result |= rd << 7;
        result |= (uint32_t)immediate << 12;
        break;
    case JUMP:
        if (!get_register(&op[0], 1, &rd, error) ||
            !get_displacement(&op[1], 2, instruction->address, symbols,
                              symbol_count, -1048576, 1048574, &immediate, error))
            return false;
        imm_bits = (uint32_t)immediate;
        /* UJ/J stores imm[20|10:1|11|19:12]; bit 0 is always zero. */
        result |= rd << 7;
        result |= ((imm_bits >> 20) & 0x1) << 31;
        result |= ((imm_bits >> 1) & 0x3ff) << 21;
        result |= ((imm_bits >> 11) & 0x1) << 20;
        result |= ((imm_bits >> 12) & 0xff) << 12;
        break;
    case FENCE: {
        int64_t predecessor_mask = 0;
        int64_t successor_mask = 0;
        if (!get_immediate(&op[0], 1, 0, 15, &predecessor_mask, error) ||
            !get_immediate(&op[1], 2, 0, 15, &successor_mask, error))
            return false;
        result |= (uint32_t)predecessor_mask << 24;
        result |= (uint32_t)successor_mask << 20;
        break;
    }
    case SYSTEM:
        break;
    }

    *word = result;
    return true;
}

/* Convert an extracted two's-complement field without signed bit shifts. */
static int64_t signed_field(uint32_t bits, unsigned width)
{
    uint32_t sign_bit = UINT32_C(1) << (width - 1);
    if ((bits & sign_bit) != 0)
        return (int64_t)bits - (INT64_C(1) << width);
    return (int64_t)bits;
}

static RvOperand register_operand(unsigned number)
{
    RvOperand operand = { .kind = RV_REGISTER, .value.reg = number };
    return operand;
}

static RvOperand immediate_operand(int64_t value)
{
    RvOperand operand = { .kind = RV_IMMEDIATE, .value.immediate = value };
    return operand;
}

static RvOperand memory_operand(unsigned base, int64_t offset)
{
    RvOperand operand = {
        .kind = RV_MEMORY, .value.memory = { .base = base, .offset = offset }
    };
    return operand;
}

bool rv64i_decode(uint32_t word, uint64_t address,
                  RvInstruction *instruction, RvError *error)
{
    if (error != NULL) {
        error->line = 0;
        error->message[0] = '\0';
    }
    if (instruction == NULL)
        return fail(error, "decoded instruction output is required");
    if (address % 4 != 0)
        return fail(error, "instruction address must be a multiple of 4");

    uint32_t opcode = word & 0x7f;
    uint32_t funct3 = (word >> 12) & 0x7;
    unsigned rd = (word >> 7) & 0x1f;
    unsigned rs1 = (word >> 15) & 0x1f;
    unsigned rs2 = (word >> 20) & 0x1f;

    for (size_t i = 0; i < sizeof encodings / sizeof encodings[0]; ++i) {
        const Encoding *encoding = &encodings[i];
        if (encoding->opcode != opcode)
            continue;
        /* U and J use bits 14:12 for the immediate, not funct3. */
        if (encoding->form != UPPER && encoding->form != JUMP &&
            encoding->funct3 != funct3)
            continue;

        RvInstruction decoded = {
            .address = address, .line = 0, .mnemonic = encoding->name
        };
        uint32_t imm_bits = 0;
        switch (encoding->form) {
        case R:
            decoded.operand_count = 3;
            decoded.operands[0] = register_operand(rd);
            decoded.operands[1] = register_operand(rs1);
            decoded.operands[2] = register_operand(rs2);
            break;
        case I: case SHIFT64: case SHIFT32: {
            int64_t immediate = signed_field(word >> 20, 12);
            if (encoding->form == SHIFT64)
                immediate = (word >> 20) & 0x3f;
            else if (encoding->form == SHIFT32)
                immediate = (word >> 20) & 0x1f;
            decoded.operand_count = 3;
            decoded.operands[0] = register_operand(rd);
            decoded.operands[1] = register_operand(rs1);
            decoded.operands[2] = immediate_operand(immediate);
            break;
        }
        case LOAD: case JALR:
            decoded.operand_count = 2;
            decoded.operands[0] = register_operand(rd);
            decoded.operands[1] = memory_operand(rs1, signed_field(word >> 20, 12));
            break;
        case STORE:
            imm_bits = ((word >> 25) & 0x7f) << 5;
            imm_bits |= (word >> 7) & 0x1f;
            decoded.operand_count = 2;
            decoded.operands[0] = register_operand(rs2);
            decoded.operands[1] = memory_operand(rs1, signed_field(imm_bits, 12));
            break;
        case BRANCH:
            imm_bits = ((word >> 31) & 0x1) << 12;
            imm_bits |= ((word >> 25) & 0x3f) << 5;
            imm_bits |= ((word >> 8) & 0xf) << 1;
            imm_bits |= ((word >> 7) & 0x1) << 11;
            decoded.operand_count = 3;
            decoded.operands[0] = register_operand(rs1);
            decoded.operands[1] = register_operand(rs2);
            decoded.operands[2] = immediate_operand(signed_field(imm_bits, 13));
            break;
        case UPPER:
            decoded.operand_count = 2;
            decoded.operands[0] = register_operand(rd);
            decoded.operands[1] = immediate_operand(word >> 12);
            break;
        case JUMP:
            imm_bits = ((word >> 31) & 0x1) << 20;
            imm_bits |= ((word >> 21) & 0x3ff) << 1;
            imm_bits |= ((word >> 20) & 0x1) << 11;
            imm_bits |= ((word >> 12) & 0xff) << 12;
            decoded.operand_count = 2;
            decoded.operands[0] = register_operand(rd);
            decoded.operands[1] = immediate_operand(signed_field(imm_bits, 21));
            break;
        case FENCE:
            decoded.operand_count = 2;
            decoded.operands[0] = immediate_operand((word >> 24) & 0xf);
            decoded.operands[1] = immediate_operand((word >> 20) & 0xf);
            break;
        case SYSTEM:
            decoded.operand_count = 0;
            break;
        }

        /* Exact reconstruction rejects unsupported funct bits and reserved
         * FENCE/SYSTEM fields instead of silently discarding those bits. */
        uint32_t reconstructed = 0;
        if (rv64i_encode(&decoded, NULL, 0, &reconstructed, NULL) &&
            reconstructed == word) {
            *instruction = decoded;
            return true;
        }
    }
    return fail(error, "unsupported or noncanonical instruction word: 0x%08" PRIx32, word);
}

bool rv64i_format(const RvInstruction *instruction,
                  char *text, size_t size, RvError *error)
{
    if (error != NULL) {
        error->line = instruction == NULL ? 0 : instruction->line;
        error->message[0] = '\0';
    }
    if (text == NULL || size == 0)
        return fail(error, "text output and nonzero buffer size are required");
    uint32_t word = 0;
    if (!rv64i_encode(instruction, NULL, 0, &word, error))
        return false;

    char formatted[128];
    int count = snprintf(formatted, sizeof formatted, "%s", instruction->mnemonic);
    if (count < 0 || (size_t)count >= sizeof formatted)
        return fail(error, "could not format instruction");
    size_t used = (size_t)count;
    bool upper = strcmp(instruction->mnemonic, "lui") == 0 ||
                 strcmp(instruction->mnemonic, "auipc") == 0;

    for (size_t i = 0; i < instruction->operand_count; ++i) {
        const RvOperand *operand = &instruction->operands[i];
        const char *separator = i == 0 ? " " : ", ";
        size_t available = sizeof formatted - used;
        switch (operand->kind) {
        case RV_REGISTER:
            count = snprintf(formatted + used, available, "%sx%u",
                             separator, operand->value.reg);
            break;
        case RV_IMMEDIATE:
            if (upper)
                count = snprintf(formatted + used, available, "%s0x%" PRIx64,
                                 separator, (uint64_t)operand->value.immediate);
            else
                count = snprintf(formatted + used, available, "%s%" PRId64,
                                 separator, operand->value.immediate);
            break;
        case RV_MEMORY:
            count = snprintf(formatted + used, available, "%s%" PRId64 "(x%u)",
                             separator, operand->value.memory.offset,
                             operand->value.memory.base);
            break;
        default:
            return fail(error, "resolve labels before formatting an instruction");
        }
        if (count < 0 || (size_t)count >= available)
            return fail(error, "could not format instruction");
        used += (size_t)count;
    }
    if (used >= size)
        return fail(error, "formatted instruction does not fit output buffer");
    memcpy(text, formatted, used + 1);
    return true;
}
