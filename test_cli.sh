#!/bin/sh
set -eu
converter=${1:-./build/riscv}
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/riscv-test.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
checks=0

check() {
    "$@" || { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
    checks=$((checks + 1))
}

reject() {
    if "$converter" "$@" >"$test_dir/stdout" 2>"$test_dir/stderr"; then
        printf 'FAIL: expected rejection: %s\n' "$*" >&2
        exit 1
    fi
    check test -s "$test_dir/stderr"
}

"$converter" assemble examples/demo.s "$test_dir/demo.hex" --base 0x1000
check cmp examples/demo.hex "$test_dir/demo.hex"
"$converter" disassemble "$test_dir/demo.hex" "$test_dir/demo.s" --base 0x1000
check grep -q '^bne x9, x24, 12$' "$test_dir/demo.s"
check grep -q '^addi x5, x0, -101$' "$test_dir/demo.s"
"$converter" assemble "$test_dir/demo.s" "$test_dir/again.hex" --base 0x1000
check cmp "$test_dir/demo.hex" "$test_dir/again.hex"

"$converter" assemble examples/all-rv64i.s "$test_dir/all.hex"
check cmp examples/all-rv64i.hex "$test_dir/all.hex"
"$converter" disassemble "$test_dir/all.hex" "$test_dir/all.s"
"$converter" assemble "$test_dir/all.s" "$test_dir/roundtrip.hex"
check cmp "$test_dir/all.hex" "$test_dir/roundtrip.hex"
"$converter" assemble examples/all-rv64i.s "$test_dir/all.bin" --format bin
check test "$(wc -l < "$test_dir/all.bin" | tr -d ' ')" = 52
"$converter" disassemble "$test_dir/all.bin" "$test_dir/from-bin.s" --format bin
"$converter" assemble "$test_dir/from-bin.s" "$test_dir/from-bin.hex"
check cmp examples/all-rv64i.hex "$test_dir/from-bin.hex"

printf 'keep this file\n' > "$test_dir/existing.hex"
reject assemble examples/demo.s "$test_dir/existing.hex"
check grep -q '^keep this file$' "$test_dir/existing.hex"
reject assemble examples/bad-immediate.s "$test_dir/invalid.hex"
check test ! -e "$test_dir/invalid.hex"
check grep -q ':1:' "$test_dir/stderr"
printf 'addi x1, x0, 1\nbne x1, x2, missing\n' > "$test_dir/undefined.s"
reject assemble "$test_dir/undefined.s" "$test_dir/undefined.hex"
check test ! -e "$test_dir/undefined.hex"
check grep -q ':2:' "$test_dir/stderr"
printf 'same: ecall\nsame: ebreak\n' > "$test_dir/duplicate.s"
reject assemble "$test_dir/duplicate.s" "$test_dir/duplicate.hex"
check test ! -e "$test_dir/duplicate.hex"

printf '00000073\nFFFFFFFF\n' > "$test_dir/invalid-word.hex"
reject disassemble "$test_dir/invalid-word.hex" "$test_dir/invalid-word.s"
check test ! -e "$test_dir/invalid-word.s"
check grep -q ':2:' "$test_dir/stderr"
printf '123\n' > "$test_dir/short.hex"
reject disassemble "$test_dir/short.hex" "$test_dir/short.s"
printf '# comment\n  0xF9B00293 # -101\n' > "$test_dir/comment.hex"
"$converter" disassemble "$test_dir/comment.hex" "$test_dir/comment.s"
check grep -q '^addi x5, x0, -101$' "$test_dir/comment.s"
printf '00000073\000garbage\n' > "$test_dir/nul.hex"
reject disassemble "$test_dir/nul.hex" "$test_dir/nul.s"
reject assemble examples/demo.s "$test_dir/base.hex" --base 2
reject assemble examples/demo.s "$test_dir/overflow.hex" --base 18446744073709551616
reject assemble examples/demo.s "$test_dir/wrap.hex" --base 0xfffffffffffffffc
reject assemble examples/demo.s "$test_dir/format.hex" --format raw
reject disassemble "$test_dir/all.bin" "$test_dir/wrong-format.s"
reject assemble "$test_dir/missing.s" "$test_dir/missing.hex"
reject assemble examples/demo.s "$test_dir/not-a-directory/output.hex"

: > "$test_dir/empty.s"
"$converter" assemble "$test_dir/empty.s" "$test_dir/empty.hex"
check test ! -s "$test_dir/empty.hex"
"$converter" --help > "$test_dir/help"
check grep -q 'assemble INPUT OUTPUT' "$test_dir/help"
printf '%s CLI checks passed\n' "$checks"
