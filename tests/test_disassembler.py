"""
Round-trip disassembler tests.

Strategy for each test:
  1. Assemble the original source  -> binary
  2. Run the binary                -> expected output / exit code
  3. Disassemble the binary        -> source text
  4. Re-assemble the disassembly   -> rebuilt binary
  5. Run the rebuilt binary        -> actual output / exit code
  6. Assert actual == expected

The disassembler emits raw register numbers (r0-r63) and raw immediates, so
the rebuilt source is not identical to the original, but must be functionally
equivalent.
"""

import os
import subprocess
import tempfile

import pytest

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VM           = os.path.join(PROJECT_ROOT, "cortex")


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _run(args, stdin="", timeout=10):
    return subprocess.run(
        args, input=stdin, capture_output=True, text=True, timeout=timeout
    )


def round_trip(source: str, stdin: str = "") -> tuple[str, int, str, int]:
    """
    Assemble + run the source, then disassemble + re-assemble + run.
    Returns (original_stdout, original_rc, rebuilt_stdout, rebuilt_rc).
    """
    with tempfile.TemporaryDirectory() as tmp:
        src_path     = os.path.join(tmp, "orig.s")
        bin_path     = os.path.join(tmp, "orig.out")
        disasm_path  = os.path.join(tmp, "disasm.s")
        rebuilt_path = os.path.join(tmp, "rebuilt.out")

        with open(src_path, "w") as f:
            f.write(source)

        # 1. assemble
        r = _run([VM, "-a", src_path, "-o", bin_path])
        assert r.returncode == 0, f"Assembly failed:\n{r.stderr}"

        # 2. run original
        orig = _run([VM, bin_path], stdin=stdin)

        # 3. disassemble
        r = _run([VM, "-d", bin_path, "-o", disasm_path])
        assert r.returncode == 0, f"Disassembly failed:\n{r.stderr}"

        # 4. re-assemble
        r = _run([VM, "-a", disasm_path, "-o", rebuilt_path])
        assert r.returncode == 0, (
            f"Re-assembly failed:\n{r.stderr}\n"
            f"Disassembly was:\n{open(disasm_path).read()}"
        )

        # 5. run rebuilt
        rebuilt = _run([VM, rebuilt_path], stdin=stdin)

        return orig.stdout, orig.returncode, rebuilt.stdout, rebuilt.returncode


def assert_round_trip(source: str, stdin: str = ""):
    orig_out, orig_rc, new_out, new_rc = round_trip(source, stdin=stdin)
    assert new_out == orig_out, (
        f"stdout mismatch after round-trip\n"
        f"  expected: {orig_out!r}\n"
        f"  got:      {new_out!r}"
    )
    assert new_rc == orig_rc, (
        f"exit code mismatch after round-trip (expected {orig_rc}, got {new_rc})"
    )


# ---------------------------------------------------------------------------
# session fixture
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session", autouse=True)
def require_binary():
    if not os.path.isfile(VM):
        pytest.skip(f"cortex binary not found at {VM}. Run `make` first.")


# ---------------------------------------------------------------------------
# base ISA
# ---------------------------------------------------------------------------

def test_round_trip_addi():
    assert_round_trip(
        "main:\n"
        "    addi r34, r0, 42\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_add():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 10\n"
        "    addi r33, r0, 32\n"
        "    add  r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_sub():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 100\n"
        "    addi r33, r0, 58\n"
        "    sub  r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_subi():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 50\n"
        "    subi r34, r32, 8\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_or():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 12\n"
        "    addi r33, r0, 3\n"
        "    or   r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_xor():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 0xFF\n"
        "    xori r34, r32, 0x0F\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_and():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 0xFF\n"
        "    andi r34, r32, 0x0F\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_sll():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 1\n"
        "    slli r34, r32, 8\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_srl():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 256\n"
        "    srli r34, r32, 4\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_sra():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, -256\n"
        "    srai r34, r32, 4\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_sw_lw():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 999\n"
        "    sw   r2, r32, 0\n"
        "    addi r2, r2, 1\n"
        "    subi r2, r2, 1\n"
        "    lw   r33, r2, 0\n"
        "    addi r18, r33, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_beq_taken():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 5\n"
        "    addi r33, r0, 5\n"
        "    beq  r32, r33, 2\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        "    addi r18, r0, 1\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_blt_loop():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 0\n"
        "    addi r33, r0, 1\n"
        "    addi r34, r0, 6\n"
        "    add  r32, r32, r33\n"
        "    addi r33, r33, 1\n"
        "    blt  r33, r34, -2\n"
        "    addi r18, r32, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_jmp():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 7\n"
        "    jmp  r0, r0, 4\n"
        "    addi r32, r0, 0\n"
        "    addi r18, r32, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_syscalls():
    assert_round_trip(
        "main:\n"
        "    nop\n"
        "    addi r18, r0, 65\n"
        "    addi r31, r0, 3\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_exit_code():
    _, _, _, rc = round_trip(
        "main:\n"
        "    addi r18, r0, 77\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )
    assert rc == 77

def test_round_trip_halt():
    assert_round_trip(
        "main:\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    halt\n"
        "    syscall\n"
    )


# ---------------------------------------------------------------------------
# M extension
# ---------------------------------------------------------------------------

def test_round_trip_mul():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 6\n"
        "    addi r33, r0, 7\n"
        "    mul  r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_muli():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 9\n"
        "    muli r34, r32, 9\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_div():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 100\n"
        "    addi r33, r0, 4\n"
        "    div  r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_rem():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 17\n"
        "    addi r33, r0, 5\n"
        "    rem  r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_mulh():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 3\n"
        "    addi r33, r0, 4\n"
        "    mulh r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_divu():
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, 99\n"
        "    addi r33, r0, 3\n"
        "    divu r34, r32, r33\n"
        "    addi r18, r34, 0\n"
        "    addi r19, r0, 0\n"
        "    addi r31, r0, 2\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )


# ---------------------------------------------------------------------------
# F extension
# ---------------------------------------------------------------------------

def test_round_trip_fadd():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 1.5\n"
        "    faddi r33, r0, 2.5\n"
        "    fadd  r34, r32, r33\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fsub():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 5.0\n"
        "    faddi r33, r0, 3.0\n"
        "    fsub  r34, r32, r33\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fmul():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 3.0\n"
        "    faddi r33, r0, 4.0\n"
        "    fmul  r34, r32, r33\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fdiv():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 10.0\n"
        "    faddi r33, r0, 4.0\n"
        "    fdiv  r34, r32, r33\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fsqrt():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 16.0\n"
        "    fsqrt r34, r32\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fabs():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, -9.0\n"
        "    fabs  r34, r32\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fneg():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 3.5\n"
        "    fneg  r34, r32\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_ftoi():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 7.9\n"
        "    ftoi  r34, r32\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 0\n"
        "    addi  r31, r0, 1\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_itof():
    assert_round_trip(
        "main:\n"
        "    addi  r32, r0, 25\n"
        "    itof  r34, r32\n"
        "    addi  r18, r34, 0\n"
        "    addi  r19, r0, 2\n"
        "    addi  r31, r0, 4\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fblt_taken():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 1.0\n"
        "    faddi r33, r0, 2.0\n"
        "    fblt  r32, r33, 2\n"
        "    addi  r18, r0, 0\n"
        "    addi  r31, r0, 0\n"
        "    syscall\n"
        "    addi  r18, r0, 1\n"
        "    addi  r31, r0, 0\n"
        "    syscall\n"
    )

def test_round_trip_fbge_not_taken():
    assert_round_trip(
        "main:\n"
        "    faddi r32, r0, 1.0\n"
        "    faddi r33, r0, 2.0\n"
        "    fbge  r32, r33, 2\n"
        "    addi  r18, r0, 99\n"
        "    addi  r31, r0, 0\n"
        "    syscall\n"
        "    addi  r18, r0, 0\n"
        "    addi  r31, r0, 0\n"
        "    syscall\n"
    )


# ---------------------------------------------------------------------------
# .data section
# ---------------------------------------------------------------------------

def test_data_single_string():
    # Print one string stored in the data section.
    assert_round_trip(
        "main:\n"
        "    addi r18, r0, greeting\n"
        "    addi r31, r0, 5\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    greeting: \"hello world\"\n"
    )

def test_data_multiple_strings():
    # Print two strings stored in the data section, one after the other.
    assert_round_trip(
        "main:\n"
        "    addi r18, r0, first\n"
        "    addi r31, r0, 5\n"
        "    syscall\n"
        "    addi r18, r0, second\n"
        "    addi r31, r0, 5\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    first:  \"foo\"\n"
        "    second: \"bar\"\n"
    )

def test_data_string_escape_newline():
    # String containing an embedded newline escape.
    assert_round_trip(
        "main:\n"
        "    addi r18, r0, msg\n"
        "    addi r31, r0, 5\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"line1\\nline2\"\n"
    )

def test_data_string_escape_tab():
    # String containing an embedded tab escape.
    assert_round_trip(
        "main:\n"
        "    addi r18, r0, msg\n"
        "    addi r31, r0, 5\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg: \"col1\\tcol2\"\n"
    )

def test_data_integer_as_exit_code():
    # Load an integer from the data section and use it as exit code.
    _, _, _, rc = round_trip(
        "main:\n"
        "    addi r32, r0, answer\n"
        "    lw   r18, r32, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    answer: 42\n"
    )
    assert rc == 42

def test_data_multiple_integers():
    # Load two integers from data, add them, use as exit code.
    _, _, _, rc = round_trip(
        "main:\n"
        "    addi r32, r0, x\n"
        "    lw   r33, r32, 0\n"
        "    addi r32, r0, y\n"
        "    lw   r34, r32, 0\n"
        "    add  r18, r33, r34\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    x: 30\n"
        "    y: 12\n"
    )
    assert rc == 42

def test_data_string_then_integer():
    # Mixed data section: a string followed by an integer used as exit code.
    _, _, _, rc = round_trip(
        "main:\n"
        "    addi r18, r0, msg\n"
        "    addi r31, r0, 5\n"
        "    syscall\n"
        "    addi r32, r0, code\n"
        "    lw   r18, r32, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    msg:  \"done\"\n"
        "    code: 7\n"
    )
    assert rc == 7

def test_data_char_by_char():
    # Walk the data section word-by-word and print each character individually.
    assert_round_trip(
        "main:\n"
        "    addi r32, r0, str\n"
        "    lw   r33, r32, 0\n"
        "    addi r18, r33, 0\n"
        "    addi r31, r0, 3\n"
        "    syscall\n"
        "    addi r32, r0, str\n"
        "    lw   r33, r32, 1\n"
        "    addi r18, r33, 0\n"
        "    addi r31, r0, 3\n"
        "    syscall\n"
        "    addi r18, r0, 0\n"
        "    addi r31, r0, 0\n"
        "    syscall\n"
        ".data\n"
        "    str: \"hi\"\n"
    )
