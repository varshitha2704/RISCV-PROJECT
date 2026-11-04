#include "parser.h"
#include "rv64i.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fputs("Usage: riscv assemble INPUT OUTPUT [--format hex|bin] [--base ADDRESS]\n"
          "       riscv disassemble INPUT OUTPUT [--format hex|bin] [--base ADDRESS]\n"
          "Defaults: hex text, base address 0. Output must be a new file.\n", out);
}

static bool read_address(const char *text, uint64_t *value)
{
    if (*text == '\0' || *text == '-' || *text == '+' || isspace((unsigned char)*text))
        return false;
    errno = 0;
    char *end;
    int base = text[0] == '0' && (text[1] == 'x' || text[1] == 'X') ? 16 : 10;
    unsigned long long number = strtoull(text, &end, base);
    if (errno == ERANGE || end == text || *end != '\0' || number > UINT64_MAX)
        return false;
    *value = (uint64_t)number;
    return *value % 4 == 0;
}

static bool write_word(FILE *out, uint32_t word, bool binary)
{
    if (!binary)
        return fprintf(out, "%08" PRIX32 "\n", word) >= 0;
    char bits[34];
    for (unsigned i = 0; i < 32; ++i)
        bits[i] = (char)('0' + ((word >> (31 - i)) & 1));
    bits[32] = '\n';
    bits[33] = '\0';
    return fputs(bits, out) != EOF;
}

static bool assemble(FILE *in, FILE *out, uint64_t base, bool binary, RvError *error)
{
    RvProgram program = {0};
    if (!rv_parse(in, base, &program, error))
        return false;
    bool ok = true;
    for (size_t i = 0; i < program.instruction_count; ++i) {
        uint32_t word;
        if (!rv64i_encode(&program.instructions[i], program.symbols,
                         program.symbol_count, &word, error)) {
            ok = false;
            break;
        }
        if (!write_word(out, word, binary)) {
            error->line = program.instructions[i].line;
            snprintf(error->message, sizeof error->message, "cannot stage output");
            ok = false;
            break;
        }
    }
    rv_program_free(&program);
    return ok;
}

static bool parse_word(const char *text, bool binary, uint32_t *word)
{
    if (text[0] == '0' && (binary ? text[1] == 'b' || text[1] == 'B'
                                : text[1] == 'x' || text[1] == 'X'))
        text += 2;
    if (strlen(text) != (binary ? 32u : 8u))
        return false;
    uint32_t result = 0;
    for (; *text; ++text) {
        unsigned digit;
        if (*text >= '0' && *text <= '9')
            digit = (unsigned)(*text - '0');
        else if (*text >= 'a' && *text <= 'f')
            digit = (unsigned)(*text - 'a') + 10;
        else if (*text >= 'A' && *text <= 'F')
            digit = (unsigned)(*text - 'A') + 10;
        else
            return false;
        if (digit >= (binary ? 2u : 16u))
            return false;
        result = (result << (binary ? 1 : 4)) | digit;
    }
    *word = result;
    return true;
}

static bool disassemble(FILE *in, FILE *out, uint64_t base, bool binary, RvError *error)
{
    char line[4096];
    uint64_t address = base;
    bool address_exhausted = false;
    size_t line_number = 0;
    int first;
    while ((first = fgetc(in)) != EOF) {
        error->line = ++line_number;
        size_t length = 0;
        int ch = first;
        while (ch != '\n' && ch != EOF) {
            if (ch == '\0' || length == sizeof line - 2) {
                snprintf(error->message, sizeof error->message,
                         ch == '\0' ? "NUL byte in text input" : "line exceeds 4094 characters");
                return false;
            }
            line[length++] = (char)ch;
            ch = fgetc(in);
        }
        line[length] = '\0';
        char *comment = strchr(line, '#');
        if (comment != NULL)
            *comment = '\0';
        char *start = line;
        while (isspace((unsigned char)*start))
            ++start;
        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1]))
            *--end = '\0';
        if (*start == '\0')
            continue;
        if (address_exhausted) {
            snprintf(error->message, sizeof error->message, "instruction address exceeds RV64 address space");
            return false;
        }
        uint32_t word;
        if (!parse_word(start, binary, &word)) {
            snprintf(error->message, sizeof error->message, "expected %s per line",
                     binary ? "32 binary digits" : "8 hexadecimal digits");
            return false;
        }
        RvInstruction instruction;
        char text[128];
        if (!rv64i_decode(word, address, &instruction, error)) {
            error->line = line_number;
            return false;
        }
        instruction.line = line_number;
        if (!rv64i_format(&instruction, text, sizeof text, error))
            return false;
        if (fprintf(out, "%s\n", text) < 0) {
            error->line = line_number;
            snprintf(error->message, sizeof error->message, "cannot stage output");
            return false;
        }
        address_exhausted = address > UINT64_MAX - 4;
        if (!address_exhausted)
            address += 4;
    }
    if (ferror(in)) {
        error->line = line_number;
        snprintf(error->message, sizeof error->message, "cannot read input");
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return EXIT_SUCCESS;
    }
    if (argc < 4 || (strcmp(argv[1], "assemble") != 0 && strcmp(argv[1], "disassemble") != 0)) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    bool binary = false;
    uint64_t base = 0;
    bool have_format = false, have_base = false;
    for (int i = 4; i < argc; i += 2) {
        if (i + 1 >= argc) {
            usage(stderr);
            return EXIT_FAILURE;
        }
        if (strcmp(argv[i], "--format") == 0 && !have_format) {
            have_format = true;
            if (strcmp(argv[i + 1], "bin") == 0)
                binary = true;
            else if (strcmp(argv[i + 1], "hex") != 0) {
                fputs("format must be hex or bin\n", stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--base") == 0 && !have_base) {
            have_base = true;
            if (!read_address(argv[i + 1], &base)) {
                fputs("base must be an unsigned, four-byte-aligned decimal or hexadecimal address\n", stderr);
                return EXIT_FAILURE;
            }
        } else {
            usage(stderr);
            return EXIT_FAILURE;
        }
    }
    FILE *in = fopen(argv[2], "r");
    if (in == NULL) {
        perror(argv[2]);
        return EXIT_FAILURE;
    }
    /* Validate the complete input before creating the requested output file. */
    FILE *staged = tmpfile();
    if (staged == NULL) {
        perror("temporary output");
        fclose(in);
        return EXIT_FAILURE;
    }
    RvError error = {0};
    bool ok = strcmp(argv[1], "assemble") == 0
        ? assemble(in, staged, base, binary, &error)
        : disassemble(in, staged, base, binary, &error);
    if (fclose(in) != 0 && ok) {
        snprintf(error.message, sizeof error.message, "cannot close input");
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "%s:%zu: %s\n", argv[2], error.line, error.message);
        fclose(staged);
        return EXIT_FAILURE;
    }
    if (fflush(staged) != 0 || fseek(staged, 0, SEEK_SET) != 0) {
        perror("temporary output");
        fclose(staged);
        return EXIT_FAILURE;
    }
    FILE *out = fopen(argv[3], "wx");
    if (out == NULL) {
        perror(argv[3]);
        fclose(staged);
        return EXIT_FAILURE;
    }
    char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, staged)) != 0) {
        if (fwrite(buffer, 1, count, out) != count) {
            ok = false;
            break;
        }
    }
    if (ferror(staged))
        ok = false;
    if (fclose(staged) != 0)
        ok = false;
    if (fclose(out) != 0)
        ok = false;
    if (!ok) {
        fprintf(stderr, "%s: cannot finish output\n", argv[3]);
        remove(argv[3]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
