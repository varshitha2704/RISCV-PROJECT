# RISC-V assembler/disassembler

CS5375 group project, written in C11 for 52 supported RV64I instructions.
The program converts assembly source to machine-code text and converts that
text back to canonical assembly. It includes the parser and symbol table,
encoder, decoder, and command-line file handling.

[Presentation slides](https://docs.google.com/presentation/d/1AmHmrYeVcGKib0JOrSE28Tlz5GLX_KS2bwlJ7aak7yo/edit)
· [Project report](https://docs.google.com/document/d/1BYL5kIvhPmSw7I1_P6Ba3P27T-XAGEZw8vkaFsVAL1I/edit)

RV64 means 64-bit registers. Each supported instruction is one 32-bit word.
This is a converter; it does not execute instructions or simulate a CPU.

## Build and run

You need a C11 compiler and Make. Run these commands from the project folder:

```sh
make
make test
make sanitize
```

`make` builds `build/riscv`. The sanitizer target needs a compiler that supports
AddressSanitizer and UndefinedBehaviorSanitizer. Without Make, build the converter
with:

```sh
mkdir -p build
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror rv64i.c parser.c main.c -o build/riscv
```

The command syntax is:

```text
riscv assemble INPUT OUTPUT [--format hex|bin] [--base ADDRESS]
riscv disassemble INPUT OUTPUT [--format hex|bin] [--base ADDRESS]
```

The defaults are hexadecimal text and base address zero. `--format` selects the
machine-code output format when assembling and the machine-code input format
when disassembling. Both formats are text, not raw bytes or ELF files:

- `hex`: one instruction per line, written as eight hexadecimal digits.
- `bin`: one instruction per line, written as 32 binary digits.

Machine-code input may contain blank lines, surrounding whitespace, and `#`
comments. An optional `0x` or `0b` prefix is accepted for the selected format.
`--base` accepts an unsigned decimal or hexadecimal address divisible by four.
Addresses advance by four per instruction; a file cannot continue past the
RV64 address space.

Output must be a new file. The program validates the entire input before
creating it, refuses to overwrite an existing path, and reports errors to
standard error with the input filename and source line when available.

## Try a complete conversion

This uses a new temporary directory on every run, so the output files do not
conflict with a previous demo:

```sh
run=$(mktemp -d)
./build/riscv assemble examples/demo.s "$run/demo.hex" --base 0x1000
cmp examples/demo.hex "$run/demo.hex"
./build/riscv disassemble "$run/demo.hex" "$run/decoded.s" --base 0x1000
cat "$run/decoded.s"
./build/riscv assemble "$run/decoded.s" "$run/again.hex" --base 0x1000
cmp "$run/demo.hex" "$run/again.hex"

./build/riscv assemble examples/all-rv64i.s "$run/all.bin" --format bin
./build/riscv disassemble "$run/all.bin" "$run/from-bin.s" --format bin
./build/riscv assemble "$run/from-bin.s" "$run/all.hex"
cmp examples/all-rv64i.hex "$run/all.hex"
```

A successful `cmp` prints nothing. Compare machine words after a round trip:
comments, labels, and register alias spelling are not stored in machine code.
The disassembler emits lowercase mnemonics, `x0`–`x31`, signed decimal offsets,
hexadecimal U-immediates, and numeric fence masks.

`examples/demo.s` contains eight examples covering R, I, S, SB, U, and UJ,
including `ld` and `sd`. `examples/all-rv64i.s` covers every supported mnemonic.
`make demo` runs the separate encoder walkthrough with a binary/hex table; its
assembly column is display text, not disassembler output.

## Assembly syntax

Use comma-separated operands. Mnemonics are normalized to lowercase. Register
names must be lowercase: `x0`–`x31` or the standard ABI aliases, including `fp`.
Labels are case-sensitive, may appear alone or before an instruction, and may
be referenced forward or backward. Duplicate or undefined labels are errors.

```asm
.text
.globl start
start:
    addi t0, zero, -101   # signed immediate
    sw t0, 40(a0)        # offset(base)
    bne s1, s8, finish   # label resolved relative to this branch
    add s1, s4, s5
finish:
    ebreak
```

Decimal, `0x` hexadecimal, and `0b` binary integers accept an optional sign.
Leading zeros do not select octal. Comments start with `#`. Each input line may
contain at most 4094 bytes, excluding its newline; embedded NUL bytes are
rejected.

`.text`, `.globl name`, and `.global name` are accepted as nonemitting directives.
They do not generate bytes, change instruction addresses, or create linker
metadata. Other directives, data sections, relocation expressions, local
numeric labels, and pseudoinstructions such as `li`, `nop`, and `ret` are not
supported.

Use explicit registers and operands: `jal ra, target`, `jalr ra, 0(sp)`, or the
three-operand spelling `jalr ra, sp, 0`. A bare `fence` means `fence iorw, iorw`;
fence masks can also be numbers from 0 to 15.

## Supported instructions and limits

The supported mnemonics come from the RV32I/RV64I tables in the supplied 2019
specification:

| Group | Instructions |
| --- | --- |
| Register arithmetic | `add sub sll slt sltu xor srl sra or and` |
| Word operations | `addw subw sllw srlw sraw` |
| Immediate arithmetic | `addi slti sltiu xori ori andi addiw` |
| Shifts | `slli srli srai slliw srliw sraiw` |
| Loads | `lb lh lw ld lbu lhu lwu` |
| Stores | `sb sh sw sd` |
| Branches | `beq bne blt bge bltu bgeu` |
| Upper immediates | `lui auipc` |
| Jumps | `jal jalr` |
| Other | `fence ecall ebreak` |

| Field | Accepted values |
| --- | --- |
| Register number | 0–31, including x0 destinations |
| Arithmetic immediate or memory offset | -2048 to 2047 |
| Immediate shift | 0–63; word shifts use 0–31 |
| Branch byte displacement | Even values from -4096 to 4094 |
| JAL byte displacement | Even values from -1048576 to 1048574 |
| U-immediate field | 0 to `0xfffff`; source also accepts -524288 to -1 |
| Fence mask | 0–15; I=8, O=4, R=2, W=1 |

For branch/JAL labels, displacement is `target address - instruction address`,
not PC+4. Numeric displacements are already byte offsets; do not divide them.
For `lui x1, 0x12345`, pass the unshifted field `0x12345`, not `0x12345000`.
An ordinary signed 12-bit immediate of -1 must be written as -1, not 4095.

The decoder accepts words that can be represented exactly by this supported
subset. It rejects unsupported or noncanonical words rather than discarding
unknown bits. Extensions such as M, A, floating point, compressed instructions,
CSR, `fence.i`, and the `fence.tso` spelling are outside scope.

Encodable does not always mean safe to execute. A two-byte-aligned branch/JAL
target can trap on RV64I without compressed instructions. Unaligned data offsets
and odd JALR offsets are also accepted; runtime behavior is outside this project.

## Files and module interfaces

| File | Purpose |
| --- | --- |
| `parser.c`, `parser.h` | Read source, collect labels and addresses, normalize operands |
| `rv64i.c`, `rv64i.h` | Instruction table, validation, encoding, decoding, formatting |
| `main.c` | Command options, hex/binary text files, error reporting |
| `test_rv64i.c`, `test_parser.c`, `test_decode.c` | Module tests |
| `test_cli.sh` | Complete conversions, round trips, and file-error checks |
| `examples/` | Demo, all 52 mnemonics, expected words, and invalid input |
| `demo.c` | Separate encoder walkthrough |
| `Makefile` | Build, test, sanitizer, and clean targets |
| [COURSE_ENCODING.md](COURSE_ENCODING.md) | Class examples and encoding details |

`rv_parse` takes a `FILE *`, base address, and initially zeroed `RvProgram`.
It gathers labels before validating instructions, so forward references work.
The program owns its parsed strings and arrays; release them with
`rv_program_free`. Parsing failure frees and zeroes the result.

`rv64i_encode` takes one `RvInstruction` plus a symbol array. The instruction
contains its byte address, original source line, lowercase mnemonic, and typed
register/immediate/memory/label operands. Success writes a `uint32_t`; failure
preserves the output and provides an `RvError`.

`rv64i_decode` takes a word and address, and produces an instruction with numeric
operands and a mnemonic in static storage. Its source line is zero until the
caller supplies one. `rv64i_format` turns that instruction into canonical
assembly. Both preserve their output on failure. See the headers for the exact
function signatures and ownership rules.

## Tests and references

The current checks pass in normal builds and under AddressSanitizer and
UndefinedBehaviorSanitizer:

- 666 encoder checks, including all 52 mnemonics, boundaries, labels, address
  wraparound, invalid operands, and class examples.
- 168 parser checks for source syntax, symbols, normalization, and errors.
- 514 decoder/formatter checks, including 96 reference words, reserved fields,
  signed offsets, and output-buffer limits.
- 33 command-line checks, including full hex/binary conversions, all-instruction
  round trips, invalid input, and protection of existing output files.

The 96 expected words and eight encoder demo results were previously checked
against LLVM 22.1.0's RISC-V assembler, with compression and relaxation disabled.
Rust was used only to access that reference assembler; it is not a project
build dependency. Tests check conversion results, not CPU execution.

Encoding follows `Ref 2 riscv-spec.pdf`, sections 2 and 5, and PDF pages 146–147
(printed pages 130–131). The encoding notes explain differences to watch for in
the other class handouts. `make clean` removes compiled programs.

## Due dates

Video due November 15, 2026 (10–15 minutes, MP4). Group report due December 1,
2026 through RaiderCanvas; the guidelines suggest ACM/IEEE conference format.
