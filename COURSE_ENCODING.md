# Encoding notes

These notes follow the examples in `Topic2 classnotes.pdf`. Page numbers refer
to PDF pages unless stated otherwise. The [README](README.md) covers the C
interface and accepted operand ranges.

## Encoding steps

Page 37 uses this process:

1. Identify the format.
2. Look up the opcode and funct fields.
3. Convert register aliases to register numbers and five-bit binary.
4. Put each field in its bit position.
5. Convert the 32-bit result to hex.

In `rv64i.c`, `rv64i_encode` finds the instruction in the table, checks its
operands, and fills in the fields for its format. The table separates `opcode`
and `funct3` from the extra fixed bits used by subtraction, arithmetic shifts,
and EBREAK. The `get_...` functions check operands and look up labels. The parser
handles aliases such as `s1` and passes register numbers to the encoder.

### R format: `add s1, s4, s5`

From page 37: `rd = s1 = x9`, `rs1 = s4 = x20`, and `rs2 = s5 = x21`.
ADD uses opcode `0110011`, funct3 `000`, and funct7 `0000000`.

Fields run from bit 31 on the left to bit 0 on the right:

```text
funct7   rs2    rs1    funct3  rd     opcode
0000000  10101  10100  000     01001  0110011

0000 0001 0101 1010 0000 0100 1011 0011 = 0x015A04B3
```

`rd` comes first in the assembly but occupies bits 11:7 in the instruction.

For this ADD, the table starts `result` at `0x33`. With `rd = 9`, `rs1 = 20`,
and `rs2 = 21`, the R-format case fills in each register:

```c
result |= rd << 7;
result |= rs1 << 15;
result |= rs2 << 20;
```

`<<` moves a value to its field's starting bit. `|=` adds those bits to the
result without clearing the fields already filled in. This gives `0x015A04B3`.
For a split immediate, the code first shifts the source bits down, masks off
the group it needs, then shifts that group into its instruction position.

### I format: `addi t0, zero, -101`

Page 40 uses a negative immediate. Its 12-bit two's-complement field is
`4096 - 101 = 3995 = 0xF9B`.

```text
imm[11:0]     rs1    funct3  rd     opcode
111110011011  00000  000     00101  0010011

1111 1001 1011 0000 0000 0010 1001 0011 = 0xF9B00293
```

Pass `-101` to the encoder. It checks the signed range before forming the bits.
The immediate is still 12 bits on RV64.

### SB format: branch labels

Page 46 gives these addresses:

| Instruction or label | Address |
| --- | --- |
| `loop` | `0xA0` |
| `bne s1, s8, exit` | `0xAC` |
| `beq zero, zero, loop` | `0xB4` |
| `exit` | `0xB8` |

For BNE, the displacement is `0xB8 - 0xAC = 12`, giving `0x01849663`.
For BEQ, it is `0xA0 - 0xB4 = -20`, giving `0xFE0006E3`.

Use byte displacements from the branch's own address. Do not subtract PC+4 or
divide by four. The encoder splits the displacement across the SB fields.

## The six formats

The class calls branch and jump formats SB and UJ; the specification calls
them B and J.

| Format | Fields from bit 31 down to bit 0 |
| --- | --- |
| R | `funct7, rs2, rs1, funct3, rd, opcode` |
| I | `imm[11:0], rs1, funct3, rd, opcode` |
| S | `imm[11:5], rs2, rs1, funct3, imm[4:0], opcode` |
| SB / B | `imm[12], imm[10:5], rs2, rs1, funct3, imm[4:1], imm[11], opcode` |
| U | `imm[31:12], rd, opcode` |
| UJ / J | `imm[20], imm[10:1], imm[11], imm[19:12], rd, opcode` |

- Opcode: seven bits. Registers: five bits each. Funct3: three bits. Funct7: seven bits.
- B/J displacements have an implicit zero bit 0.
- RV64 immediate shifts use six shift bits; word shifts use five.
- `jalr` uses I format. Stores, including `sd`, use S format.
- In `sw rs2, offset(rs1)`, `rs2` holds the value and `rs1` is the address base.

## Class examples

These expected words are covered by `test_class_examples` in `test_rv64i.c`.

| Topic2 notes page | Assembly / byte displacement | Word |
| --- | --- | --- |
| 37 | `add s1, s4, s5` | `015A04B3` |
| 38 | `sub t1, a3, s4` | `41468333` |
| 39 | `addi t5, s7, 100` | `064B8F13` |
| 40 | `addi t0, zero, -101` | `F9B00293` |
| 41 | `ori a0, s1, 0x7c` | `07C4E513` |
| 41 exercise | `and s0, s1, s2` | `0124F433` |
| 42 | `slli a1, s3, 4` | `00499593` |
| 43 | `srai s0, a0, 4` | `40455413` |
| 44 | `lui s3, 0x53a4b` | `53A4B9B7` |
| 45–46 | `bne s1, s8, +12` | `01849663` |
| 46 | `beq zero, zero, -20` | `FE0006E3` |
| 47, adapted | `jal ra, +0x800a0` | `0A0800EF` |
| 49 | `sw t0, 40(a0)` | `02552423` |
| 50 | `sub a1, a0, s1` | `409505B3` |
| 26 | `jalr zero, 0(ra)` | `00008067` |

The page 46 tests use the labels and addresses shown above. Page 47 leaves the
jump's source PC unclear; its word encodes displacement `0x800A0`. The adapted
test uses source `0x1000` and target `0x810A0` to make that displacement explicit.

## Differences between the references

For encoding details, use `Ref 2 riscv-spec.pdf`, sections 2 and 5 and the
instruction table on PDF pages 146–147 (printed 130–131). A few other examples
differ from the specification:

| Source | Difference | Rule used here |
| --- | --- | --- |
| Quick ref sheet BM, p2 | Labels SD as I format | SD uses S format |
| Topic 2 ISA architecture, p12 | Groups JALR under U | JALR uses I format |
| Ref 1, pp34 and 43 | Says x0 cannot be a destination | Valid x0 destinations are accepted |
| Ref 1, pp67–68 and 77 | Uses the following PC for branches/JAL | Use the current instruction address |
| Ref 1, p54 | Gives a different SRAIW shift range | Word shifts accept 0–31 |
| Topic2 classnotes, p7 | Treats `0xFB` as −5 in ADDI | `0xFB` is +251; pass −5, which encodes as `0xFFB` |
| Topic 3 Processor, pp3–4 and 21 | Uses MIPS-style fields and `NPC + (Imm << 2)` | Use RISC-V fields and byte displacements |

The MIPS-style diagrams also appear in Topic3 classnotes p4 and Various
processor architectures pp22–23. They explain pipelines, but do not define
RISC-V encodings. The references also include extensions and pseudoinstructions;
the encoder's supported RV64I list is in the README.
