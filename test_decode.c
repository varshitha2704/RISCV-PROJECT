#include "rv64i.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Independent expected words copied from the encoder's 96 LLVM-checked
 * fixtures. Expected text uses numeric registers and signed byte offsets. */
static const struct {
    uint32_t word;
    const char *text;
} examples[] = {
    { UINT32_C(0x003100b3), "add x1, x2, x3" },
    { UINT32_C(0x403100b3), "sub x1, x2, x3" },
    { UINT32_C(0x003110b3), "sll x1, x2, x3" },
    { UINT32_C(0x003120b3), "slt x1, x2, x3" },
    { UINT32_C(0x003130b3), "sltu x1, x2, x3" },
    { UINT32_C(0x003140b3), "xor x1, x2, x3" },
    { UINT32_C(0x003150b3), "srl x1, x2, x3" },
    { UINT32_C(0x403150b3), "sra x1, x2, x3" },
    { UINT32_C(0x003160b3), "or x1, x2, x3" },
    { UINT32_C(0x003170b3), "and x1, x2, x3" },
    { UINT32_C(0xfff10093), "addi x1, x2, -1" },
    { UINT32_C(0xfff12093), "slti x1, x2, -1" },
    { UINT32_C(0xfff13093), "sltiu x1, x2, -1" },
    { UINT32_C(0xfff14093), "xori x1, x2, -1" },
    { UINT32_C(0xfff16093), "ori x1, x2, -1" },
    { UINT32_C(0xfff17093), "andi x1, x2, -1" },
    { UINT32_C(0x03f11093), "slli x1, x2, 63" },
    { UINT32_C(0x03f15093), "srli x1, x2, 63" },
    { UINT32_C(0x43f15093), "srai x1, x2, 63" },
    { UINT32_C(0x00c10083), "lb x1, 12(x2)" },
    { UINT32_C(0x00c11083), "lh x1, 12(x2)" },
    { UINT32_C(0x00c12083), "lw x1, 12(x2)" },
    { UINT32_C(0x00c14083), "lbu x1, 12(x2)" },
    { UINT32_C(0x00c15083), "lhu x1, 12(x2)" },
    { UINT32_C(0x00310423), "sb x3, 8(x2)" },
    { UINT32_C(0x00311423), "sh x3, 8(x2)" },
    { UINT32_C(0x00312423), "sw x3, 8(x2)" },
    { UINT32_C(0x00208463), "beq x1, x2, 8" },
    { UINT32_C(0x00209463), "bne x1, x2, 8" },
    { UINT32_C(0x0020c463), "blt x1, x2, 8" },
    { UINT32_C(0x0020d463), "bge x1, x2, 8" },
    { UINT32_C(0x0020e463), "bltu x1, x2, 8" },
    { UINT32_C(0x0020f463), "bgeu x1, x2, 8" },
    { UINT32_C(0x008000ef), "jal x1, 8" },
    { UINT32_C(0xfff100e7), "jalr x1, -1(x2)" },
    { UINT32_C(0x123450b7), "lui x1, 0x12345" },
    { UINT32_C(0x12345097), "auipc x1, 0x12345" },
    { UINT32_C(0x0ff0000f), "fence 15, 15" },
    { UINT32_C(0x00000073), "ecall" },
    { UINT32_C(0x00100073), "ebreak" },
    { UINT32_C(0x00c13083), "ld x1, 12(x2)" },
    { UINT32_C(0x00c16083), "lwu x1, 12(x2)" },
    { UINT32_C(0x00313423), "sd x3, 8(x2)" },
    { UINT32_C(0xfff1009b), "addiw x1, x2, -1" },
    { UINT32_C(0x01f1109b), "slliw x1, x2, 31" },
    { UINT32_C(0x01f1509b), "srliw x1, x2, 31" },
    { UINT32_C(0x41f1509b), "sraiw x1, x2, 31" },
    { UINT32_C(0x003100bb), "addw x1, x2, x3" },
    { UINT32_C(0x403100bb), "subw x1, x2, x3" },
    { UINT32_C(0x003110bb), "sllw x1, x2, x3" },
    { UINT32_C(0x003150bb), "srlw x1, x2, x3" },
    { UINT32_C(0x403150bb), "sraw x1, x2, x3" },
    { UINT32_C(0x00000033), "add x0, x0, x0" },
    { UINT32_C(0x01ff8fb3), "add x31, x31, x31" },
    { UINT32_C(0x80000f93), "addi x31, x0, -2048" },
    { UINT32_C(0x7ff00f93), "addi x31, x0, 2047" },
    { UINT32_C(0x00011093), "slli x1, x2, 0" },
    { UINT32_C(0x0001109b), "slliw x1, x2, 0" },
    { UINT32_C(0x80012083), "lw x1, -2048(x2)" },
    { UINT32_C(0x7ff12083), "lw x1, 2047(x2)" },
    { UINT32_C(0x06442283), "lw x5, 100(x8)" },
    { UINT32_C(0x80312023), "sw x3, -2048(x2)" },
    { UINT32_C(0x7e312fa3), "sw x3, 2047(x2)" },
    { UINT32_C(0x800100e7), "jalr x1, -2048(x2)" },
    { UINT32_C(0x7ff100e7), "jalr x1, 2047(x2)" },
    { UINT32_C(0x000000b7), "lui x1, 0x0" },
    { UINT32_C(0xfffff0b7), "lui x1, 0xfffff" },
    { UINT32_C(0x80000063), "beq x0, x0, -4096" },
    { UINT32_C(0x7e000fe3), "beq x0, x0, 4094" },
    { UINT32_C(0x000000e3), "beq x0, x0, 2048" },
    { UINT32_C(0x00000163), "beq x0, x0, 2" },
    { UINT32_C(0xfe000fe3), "beq x0, x0, -2" },
    { UINT32_C(0x800000ef), "jal x1, -1048576" },
    { UINT32_C(0x7ffff0ef), "jal x1, 1048574" },
    { UINT32_C(0x001000ef), "jal x1, 2048" },
    { UINT32_C(0x000800ef), "jal x1, 524288" },
    { UINT32_C(0x002000ef), "jal x1, 2" },
    { UINT32_C(0xfffff0ef), "jal x1, -2" },
    { UINT32_C(0x0000000f), "fence 0, 0" },
    { UINT32_C(0x0810000f), "fence 8, 1" },
    { UINT32_C(0x0240000f), "fence 2, 4" },
    { UINT32_C(0x015a04b3), "add x9, x20, x21" },
    { UINT32_C(0x41468333), "sub x6, x13, x20" },
    { UINT32_C(0x064b8f13), "addi x30, x23, 100" },
    { UINT32_C(0xf9b00293), "addi x5, x0, -101" },
    { UINT32_C(0x07c4e513), "ori x10, x9, 124" },
    { UINT32_C(0x0124f433), "and x8, x9, x18" },
    { UINT32_C(0x00499593), "slli x11, x19, 4" },
    { UINT32_C(0x40455413), "srai x8, x10, 4" },
    { UINT32_C(0x53a4b9b7), "lui x19, 0x53a4b" },
    { UINT32_C(0x01849663), "bne x9, x24, 12" },
    { UINT32_C(0xfe0006e3), "beq x0, x0, -20" },
    { UINT32_C(0x0a0800ef), "jal x1, 524448" },
    { UINT32_C(0x02552423), "sw x5, 40(x10)" },
    { UINT32_C(0x409505b3), "sub x11, x10, x9" },
    { UINT32_C(0x00008067), "jalr x0, 0(x1)" },
};

static size_t tests_run;
static size_t tests_failed;

static void check_word_at(uint32_t word, const char *expected, uint64_t address)
{
    RvInstruction instruction;
    RvError error = { 0 };
    char text[128];
    uint32_t encoded = 0;
    ++tests_run;
    if (!rv64i_decode(word, address, &instruction, &error) ||
        instruction.address != address || instruction.line != 0 ||
        !rv64i_format(&instruction, text, sizeof text, &error) ||
        strcmp(text, expected) != 0 ||
        !rv64i_encode(&instruction, NULL, 0, &encoded, &error) || encoded != word) {
        ++tests_failed;
        fprintf(stderr, "FAIL decode/format 0x%08" PRIx32 " as %s: %s\n",
                word, expected, error.message);
    }
}

static void check_word(uint32_t word, const char *expected)
{
    check_word_at(word, expected, UINT64_C(0x1000));
}

static void reject_word(uint32_t word)
{
    RvInstruction instruction;
    unsigned char original[sizeof instruction];
    memset(&instruction, 0xa5, sizeof instruction);
    memcpy(original, &instruction, sizeof instruction);
    RvError error = { .line = 999 };
    ++tests_run;
    if (rv64i_decode(word, 0, &instruction, &error) ||
        error.line != 0 || error.message[0] == '\0' ||
        memcmp(original, &instruction, sizeof instruction) != 0) {
        ++tests_failed;
        fprintf(stderr, "FAIL expected unchanged output and error for 0x%08" PRIx32 "\n", word);
    }
}

static void check(bool condition, const char *name)
{
    ++tests_run;
    if (!condition) {
        ++tests_failed;
        fprintf(stderr, "FAIL %s\n", name);
    }
}

static void test_reserved_fields(void)
{
    const uint32_t invalid[] = {
        UINT32_C(0x00000000), /* unsupported/compressed encoding */
        UINT32_C(0xffffffff),
        UINT32_C(0x02000033), /* MUL extension */
        UINT32_C(0x02001033), /* MULH extension */
        UINT32_C(0x40001033), /* reserved SLL funct7 */
        UINT32_C(0x0000203b), /* unsupported word-operation funct3 */
        UINT32_C(0x00007003), /* reserved load width */
        UINT32_C(0x00004023), /* reserved store width */
        UINT32_C(0x00002063), /* reserved branch funct3 */
        UINT32_C(0x00001067), /* JALR requires funct3 zero */
        UINT32_C(0x00001073), /* CSR extension */
        UINT32_C(0x00200073), /* unsupported system immediate */
        UINT32_C(0x000000f3), /* ECALL with nonzero rd */
        UINT32_C(0x00008073), /* ECALL with nonzero rs1 */
        UINT32_C(0x001000f3), /* EBREAK with nonzero rd */
        UINT32_C(0x0000100f), /* FENCE.I extension */
        UINT32_C(0x0ff0008f), /* FENCE with nonzero rd */
        UINT32_C(0x0ff0800f), /* FENCE with nonzero rs1 */
        UINT32_C(0x1ff0000f), /* unsupported FENCE mode */
        UINT32_C(0x8330000f)  /* FENCE.TSO has its own mode */
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        reject_word(invalid[i]);

    /* RV64 shifts have six shift bits; W shifts have five. Check every
     * possible upper function field so a reserved bit cannot be dropped. */
    for (uint32_t funct6 = 0; funct6 < 64; ++funct6) {
        uint32_t left = UINT32_C(0x03f11093) | (funct6 << 26);
        uint32_t right = UINT32_C(0x03f15093) | (funct6 << 26);
        if (funct6 == 0) {
            check_word(left, "slli x1, x2, 63");
            check_word(right, "srli x1, x2, 63");
        } else {
            reject_word(left);
            if (funct6 == 16)
                check_word(right, "srai x1, x2, 63");
            else
                reject_word(right);
        }
    }
    for (uint32_t funct7 = 0; funct7 < 128; ++funct7) {
        uint32_t left = UINT32_C(0x01f1109b) | (funct7 << 25);
        uint32_t right = UINT32_C(0x01f1509b) | (funct7 << 25);
        if (funct7 == 0) {
            check_word(left, "slliw x1, x2, 31");
            check_word(right, "srliw x1, x2, 31");
        } else {
            reject_word(left);
            if (funct7 == 32)
                check_word(right, "sraiw x1, x2, 31");
            else
                reject_word(right);
        }
    }
}

static void test_api(void)
{
    RvInstruction instruction = { .address = 44, .line = 123, .mnemonic = "sentinel" };
    RvInstruction original = instruction;
    RvError error = { 0 };
    check(!rv64i_decode(UINT32_C(0x003100b3), 0, NULL, &error) &&
          error.line == 0 && error.message[0] != '\0', "null decoder output");
    check(!rv64i_decode(UINT32_C(0x003100b3), 2, &instruction, &error) &&
          instruction.address == original.address && instruction.mnemonic == original.mnemonic &&
          error.message[0] != '\0', "unaligned address preserves output");
    check(rv64i_decode(UINT32_C(0x003100b3), 0, &instruction, NULL),
          "optional decode error on success");
    original = instruction;
    check(!rv64i_decode(0, 0, &instruction, NULL) &&
          instruction.mnemonic == original.mnemonic && instruction.address == original.address,
          "optional decode error on failure");
    check_word_at(UINT32_C(0xfe208ce3), "beq x1, x2, -8", UINT64_MAX - 3);

    char text[128] = "unchanged";
    const char *expected = "add x1, x2, x3";
    check(!rv64i_format(&instruction, text, strlen(expected), &error) &&
          strcmp(text, "unchanged") == 0 && error.message[0] != '\0',
          "insufficient space preserves text including missing NUL space");
    check(rv64i_format(&instruction, text, strlen(expected) + 1, &error) &&
          strcmp(text, expected) == 0, "exact buffer size including NUL");
    strcpy(text, "unchanged");
    check(!rv64i_format(&instruction, text, 0, &error) && strcmp(text, "unchanged") == 0,
          "zero buffer size");
    check(!rv64i_format(&instruction, NULL, sizeof text, &error), "null text output");
    check(!rv64i_format(NULL, text, sizeof text, &error) &&
          error.line == 0 && strcmp(text, "unchanged") == 0, "null format input");
    instruction.line = 42;
    instruction.operands[0].value.reg = 32;
    check(!rv64i_format(&instruction, text, sizeof text, &error) &&
          error.line == 42 && error.message[0] != '\0' && strcmp(text, "unchanged") == 0,
          "invalid instruction fails formatting with source line");
    instruction.operands[0].value.reg = 1;
    check(rv64i_format(&instruction, text, sizeof text, NULL) && strcmp(text, expected) == 0,
          "optional formatter error on success");
    instruction.operands[0].value.reg = 32;
    check(!rv64i_format(&instruction, text, sizeof text, NULL) && strcmp(text, expected) == 0,
          "optional formatter error on failure");

    instruction = (RvInstruction){
        .address = 0, .line = 43, .mnemonic = "jal", .operand_count = 2,
        .operands = {
            { .kind = RV_REGISTER, .value.reg = 1 },
            { .kind = RV_LABEL, .value.label = "target" }
        }
    };
    check(!rv64i_format(&instruction, text, sizeof text, &error) &&
          error.line == 43 && strcmp(text, expected) == 0, "unresolved label rejected");
}

int main(void)
{
    for (size_t i = 0; i < sizeof examples / sizeof examples[0]; ++i)
        check_word(examples[i].word, examples[i].text);
    test_reserved_fields();
    test_api();
    printf("%zu/%zu decoder/formatter tests passed (96 reference words).\n",
           tests_run - tests_failed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
