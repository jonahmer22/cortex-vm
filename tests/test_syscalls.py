"""Tests for syscall instructions."""
from conftest import asm_out, run_asm, prog


# ---------------------------------------------------------------------------
# SYS_PRINT_INT (a13=1) — format controlled by a1
# ---------------------------------------------------------------------------

def test_print_int_decimal():
    src = "    addi a1, zero, 0\n    addi a13, zero, 1\n    addi a0, zero, 42\n    syscall\n"
    assert asm_out(prog(src)) == "42"


def test_print_int_decimal_negative():
    src = "    subi a0, zero, 7\n    addi a1, zero, 0\n    addi a13, zero, 1\n    syscall\n"
    assert asm_out(prog(src)) == "-7"


def test_print_int_hex():
    src = "    addi a0, zero, 255\n    addi a1, zero, 3\n    addi a13, zero, 1\n    syscall\n"
    assert asm_out(prog(src)) == "0x00000000000000FF"


def test_print_int_binary():
    # a1=1 → 64-bit binary string
    src = "    addi a0, zero, 5\n    addi a1, zero, 1\n    addi a13, zero, 1\n    syscall\n"
    out = asm_out(prog(src))
    assert out == "0b" + "0" * 61 + "101"


def test_print_int_octal():
    src = "    addi a0, zero, 8\n    addi a1, zero, 2\n    addi a13, zero, 1\n    syscall\n"
    assert asm_out(prog(src)) == "0o0000000000000000000010"


# ---------------------------------------------------------------------------
# SYS_PRINT_UINT (a13=2)
# ---------------------------------------------------------------------------

def test_print_uint_decimal():
    src = "    addi a0, zero, 1000\n    addi a1, zero, 0\n    addi a13, zero, 2\n    syscall\n"
    assert asm_out(prog(src)) == "1000"


def test_print_uint_large():
    # -1 as uint64 = 18446744073709551615
    src = "    subi a0, zero, 1\n    addi a1, zero, 0\n    addi a13, zero, 2\n    syscall\n"
    assert asm_out(prog(src)) == "18446744073709551615"


# ---------------------------------------------------------------------------
# SYS_PRINT_CHAR (a13=3)
# ---------------------------------------------------------------------------

def test_print_char():
    src = "    addi a0, zero, 'A'\n    addi a13, zero, 3\n    syscall\n"
    assert asm_out(prog(src)) == "A"


def test_print_char_newline():
    src = "    addi a0, zero, '\\n'\n    addi a13, zero, 3\n    syscall\n"
    assert asm_out(prog(src)) == "\n"


def test_print_char_digit():
    src = "    addi a0, zero, '7'\n    addi a13, zero, 3\n    syscall\n"
    assert asm_out(prog(src)) == "7"


# ---------------------------------------------------------------------------
# SYS_PRINT_STR (a13=5)
# ---------------------------------------------------------------------------

def test_print_str():
    src = "    addi a0, zero, s\n    addi a13, zero, 5\n    syscall\n"
    data = '    s: "hello world"\n'
    assert asm_out(prog(src, data=data)) == "hello world"


def test_print_str_empty():
    src = "    addi a0, zero, s\n    addi a13, zero, 5\n    syscall\n"
    data = '    s: ""\n'
    assert asm_out(prog(src, data=data)) == ""


def test_print_str_with_newline_escape():
    src = "    addi a0, zero, s\n    addi a13, zero, 5\n    syscall\n"
    data = '    s: "line1\\nline2"\n'
    assert asm_out(prog(src, data=data)) == "line1\nline2"


# ---------------------------------------------------------------------------
# SYS_PRINT_FLOAT (a13=4) — precision in a1
# ---------------------------------------------------------------------------

def test_print_float_precision_2():
    src = (
        "    lw a0, zero, v\n"
        "    addi a1, zero, 2\n"
        "    addi a13, zero, 4\n"
        "    syscall\n"
    )
    data = "    v: 3.14\n"
    assert asm_out(prog(src, data=data)) == "3.14"


def test_print_float_precision_0():
    src = (
        "    lw a0, zero, v\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 4\n"
        "    syscall\n"
    )
    data = "    v: 7.9\n"
    assert asm_out(prog(src, data=data)) == "8"


# ---------------------------------------------------------------------------
# SYS_RAND_SEED / SYS_RAND_INT (a13=21/22)
# ---------------------------------------------------------------------------

def test_nop_does_not_crash():
    src = "    nop\n    nop\n    nop\n"
    assert asm_out(prog(src)) == ""


def test_rand_int_seeded_does_not_crash():
    src = (
        "    addi a0, zero, 42\n"
        "    addi a13, zero, 21\n"    # seed
        "    syscall\n"
        "    addi a13, zero, 22\n"    # rand_int
        "    syscall\n"
        # result in a0; just verify we can print it
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
    )
    result = run_asm(prog(src))
    assert result.returncode == 0
    assert result.stdout.lstrip("-").isdigit()


def test_rand_r_int_in_range():
    # SYS_RAND_R_INT: result in [a0, a1] = [0, 100]
    src = (
        "    addi a0, zero, 1\n"
        "    addi a13, zero, 21\n"    # seed = 1
        "    syscall\n"
        "    addi a0, zero, 0\n"
        "    addi a1, zero, 100\n"
        "    addi a13, zero, 23\n"    # rand_r_int [0, 100]
        "    syscall\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
    )
    out = asm_out(prog(src))
    value = int(out)
    assert 0 <= value <= 100


def test_rand_r_int_single_value():
    # [5, 5] must always return 5
    src = (
        "    addi a0, zero, 5\n"
        "    addi a1, zero, 5\n"
        "    addi a13, zero, 23\n"
        "    syscall\n"
        "    addi a1, zero, 0\n"
        "    addi a13, zero, 1\n"
        "    syscall\n"
    )
    assert asm_out(prog(src)) == "5"


# ---------------------------------------------------------------------------
# SYS_RAND_FLOAT (a13=24)
# ---------------------------------------------------------------------------

def test_rand_float_in_range():
    src = (
        "    addi a13, zero, 24\n"
        "    syscall\n"
        "    addi a1, zero, 6\n"
        "    addi a13, zero, 4\n"
        "    syscall\n"
    )
    out = asm_out(prog(src))
    value = float(out)
    assert 0.0 <= value <= 1.0


# ---------------------------------------------------------------------------
# SYS_EXIT (a13=0) — propagates exit code
# ---------------------------------------------------------------------------

def test_sys_exit_code_0():
    src = "main:\n    addi a0, zero, 0\n    addi a13, zero, 0\n    syscall\n"
    assert run_asm(src).returncode == 0


def test_sys_exit_code_7():
    src = "main:\n    addi a0, zero, 7\n    addi a13, zero, 0\n    syscall\n"
    assert run_asm(src).returncode == 7


# ---------------------------------------------------------------------------
# Halt instruction
# ---------------------------------------------------------------------------

def test_halt_terminates():
    src = "    halt\n"
    result = run_asm(prog(src))
    assert result.returncode == 0
